#include "AutoSequencer.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <ranges>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All 28 trance fills generate valid patterns across 1 to 64 bars", "[auto]") {
    std::ranges::for_each(core::kAutoBarCounts, [](int bars) {
        const int maxSteps = bars * core::kStepsPerBar64;
        std::ranges::for_each(std::views::iota(0, core::kNumFillPresets), [&](int preset) {
            const auto notes = core::buildPresetPattern(preset, bars);
            REQUIRE(!notes.empty());
            REQUIRE(notes.front().step64 >= 0);
            REQUIRE(notes.back().step64 < maxSteps);
        });
    });
}

TEST_CASE("V3 trance fill names and patterns are distinct", "[auto]") {
    REQUIRE(core::kNumFillPresets == 28);
    REQUIRE(core::kFillPresetNames.size() == 28u);
    REQUIRE(core::kFillPresetNames[0] == "Ion Drive Four");
    REQUIRE(core::kFillPresetNames[11] == "Pulse Stamp");
    REQUIRE(core::kFillPresetNames[12] == "Helix Grid");
    REQUIRE(core::kFillPresetNames[15] == "Dawn Breaker");
    REQUIRE(core::kFillPresetNames[16] == "Luminous Arc");
    REQUIRE(core::kFillPresetNames[17] == "Front Row Ladder");
    REQUIRE(core::kFillPresetNames[18] == "Carbon Pulse");
    REQUIRE(core::kFillPresetNames[27] == "Final Orbit Reverse");

    const auto floor = core::buildPresetPattern(0, 1);
    REQUIRE_FALSE(floor.empty());
    REQUIRE(floor.front().step64 == 0);
    REQUIRE(std::ranges::is_sorted(floor, {}, &core::PianoRollNote::step64));

    const auto signature = [](int preset) {
        std::string text;
        for (const auto& note : core::buildPresetPattern(preset, 4)) {
            text += std::to_string(note.step64);
            text += ":";
            text += note.reverseCue ? "r;" : "f;";
        }
        return text;
    };
    std::vector<std::string> signatures;
    for (int preset = 0; preset < core::kNumFillPresets; ++preset) {
        signatures.push_back(signature(preset));
        REQUIRE(!signatures.back().empty());
    }
    std::ranges::sort(signatures);
    REQUIRE(std::ranges::adjacent_find(signatures) == signatures.end());
    REQUIRE(std::ranges::any_of(core::buildPresetPattern(8, 2), [](const core::PianoRollNote& note) {
        return note.reverseCue;
    }));
}

TEST_CASE("1/64-note build-up presets contain adjacent 1/64-note kick steps", "[auto]") {
    const auto rushNotes = core::buildPresetPattern(25, 4);
    const bool hasSixtyFourthStep = std::ranges::any_of(
        std::views::iota(size_t{ 1 }, rushNotes.size()),
        [&](size_t idx) { return (rushNotes[idx].step64 - rushNotes[idx - 1u].step64) == 1; });
    REQUIRE(hasSixtyFourthStep);
}

TEST_CASE("Auto tail shortener tightens kick tail on rapid notes to prevent mud", "[auto]") {
    const float baseTailMs = 420.0f;
    const double sixtyFourthNoteMsAt128Bpm = (60000.0 / 128.0) / 16.0; // ~29.3 ms

    const float unshortened = core::computeAutoEffectiveTailMs(baseTailMs, 0.0f, sixtyFourthNoteMsAt128Bpm);
    const float shortened = core::computeAutoEffectiveTailMs(baseTailMs, 0.85f, sixtyFourthNoteMsAt128Bpm);

    REQUIRE_THAT(unshortened, WithinAbs(baseTailMs, 0.001f));
    REQUIRE(shortened < 100.0f);
    REQUIRE(shortened >= 15.0f);
}

TEST_CASE("Piano roll note toggle adds and removes kick hits at 1/64 resolution", "[auto]") {
    core::AutoSequencer seq;
    seq.setNotesForSlot(0, {});
    REQUIRE(seq.notesForSlot(0).empty());

    REQUIRE(seq.toggleNoteAt(0, 63, 1, 1));
    REQUIRE(seq.notesForSlot(0).size() == 1u);
    REQUIRE(seq.notesForSlot(0).front().step64 == 63);
    REQUIRE(!seq.toggleNoteAt(0, 63, 1, 1));
    REQUIRE(seq.notesForSlot(0).empty());
}

TEST_CASE("Layer lanes schedule simultaneous hits and sample accurate ratchets", "[layer-improvements]") {
    openedmkick::core::AutoSequencer sequencer;
    sequencer.setNotesForSlot(0, {{0, 4, 0.8f, false, 0, 4}, {0, 4, 0.3f, false, 2, 1}});
    openedmkick::core::AutoSlotSettings settings;
    settings.bars = 1;
    const auto first = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0);
    REQUIRE(first.layerMask == 5);
    REQUIRE(first.layerVelocities[0] == 0.8f);
    REQUIRE(first.layerVelocities[2] == 0.3f);
    const auto between = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0.03);
    REQUIRE_FALSE(between.triggered);
    const auto repeat = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0.0625);
    REQUIRE(repeat.layerMask == 1);
    REQUIRE(repeat.layerVelocities[0] == 0.8f);
    const auto noDuplicate = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0.06251);
    REQUIRE_FALSE(noDuplicate.triggered);
}

TEST_CASE("Legacy lane wraps tail spacing at the scene boundary", "[layer-improvements]") {
    openedmkick::core::AutoSequencer sequencer;
    sequencer.setNotesForSlot(0, {{0, 1, 1, false}, {63, 1, 1, false}});
    openedmkick::core::AutoSlotSettings settings;
    settings.bars = 1;
    const auto last = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 63.0 / 16);
    REQUIRE(last.stepDurationBeats == 1.0 / 16);
}

TEST_CASE("Triplet ratchets keep exact sample timing", "[layer-improvements]") {
    openedmkick::core::AutoSequencer sequencer;
    sequencer.setNotesForSlot(0, {{0, 4, 1, false, 1, 3}});
    openedmkick::core::AutoSlotSettings settings;
    settings.bars = 1;
    std::vector<int> positions;
    for (int sample = 0; sample < 6000; ++sample) {
        const auto hit = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, static_cast<double>(sample) / 24000);
        if (hit.triggered) positions.push_back(sample);
    }
    REQUIRE(positions == std::vector<int>{0, 2000, 4000});
}

TEST_CASE("Explicit layer hits override a shared hit's velocity", "[layer-improvements]") {
    openedmkick::core::AutoSequencer sequencer;
    sequencer.setNotesForSlot(0, {{0, 4, 0.2f, false, 2, 1}, {0, 4, 1, false, -1, 1}});
    openedmkick::core::AutoSlotSettings settings;
    const auto hit = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0);
    REQUIRE(hit.layerMask == 15);
    REQUIRE(hit.layerVelocities[0] == 1);
    REQUIRE(hit.layerVelocities[2] == 0.2f);
}

TEST_CASE("Simultaneous shared and layer notes retain the next hit's tail spacing", "[layer-improvements]") {
    openedmkick::core::AutoSequencer sequencer;
    sequencer.setNotesForSlot(0, {{0, 4, 1, false, -1}, {0, 4, 0.2f, false, 2}, {4, 4, 1, false, -1}});
    openedmkick::core::AutoSlotSettings settings;
    settings.bars = 1;
    settings.tailShorten = 1;
    const auto hit = sequencer.stepSample(0, settings, 300, 120, 4, 48000, true, 0);
    REQUIRE(hit.layerTailMs[0] == openedmkick::core::computeAutoEffectiveTailMs(300, 1, 125));
}
