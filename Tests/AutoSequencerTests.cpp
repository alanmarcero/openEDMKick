#include "AutoSequencer.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <ranges>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All 16 fill presets generate valid patterns across 1 to 64 bars", "[auto]") {
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

TEST_CASE("Original fill indexes keep their patterns and the new ones are separate", "[auto]") {
    REQUIRE(core::kNumFillPresets == 28);
    REQUIRE(core::kFillPresetNames.size() == 28u);
    REQUIRE(core::kFillPresetNames[0] == "Four on the Floor + End Fill");
    REQUIRE(core::kFillPresetNames[11] == "Mainstage 64th Machine Gun");
    REQUIRE(core::kFillPresetNames[12] == "Offbeat House Pump");
    REQUIRE(core::kFillPresetNames[15] == "Call And Answer Reverse");
    REQUIRE(core::kFillPresetNames[16] == "Trance 138 Rolling Sub Fill");
    REQUIRE(core::kFillPresetNames[27] == "Hands-Up 16th Staccato");

    const auto floor = core::buildPresetPattern(0, 1);
    REQUIRE(floor.size() == 8u);
    REQUIRE(floor[0].step64 == 0);
    REQUIRE(floor[1].step64 == 16);
    REQUIRE(floor[2].step64 == 32);
    REQUIRE(floor[3].step64 == 48);
    REQUIRE(floor[6].step64 == 60);
    REQUIRE(floor[7].step64 == 62);
    REQUIRE(floor[7].semitoneOffset == 5);
    REQUIRE(!floor[0].reverseCue);

    const auto signature = [](int preset) {
        std::string text;
        for (const auto& note : core::buildPresetPattern(preset, 4)) {
            text += std::to_string(note.step64);
            text += ":";
            text += std::to_string(note.semitoneOffset);
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
    REQUIRE(std::ranges::any_of(core::buildPresetPattern(15, 2), [](const core::PianoRollNote& note) {
        return note.reverseCue;
    }));
}

TEST_CASE("1/64-note build-up presets contain adjacent 1/64-note kick steps", "[auto]") {
    const auto rushNotes = core::buildPresetPattern(2, 4);
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

TEST_CASE("Piano roll note toggle adds, updates pitch row, and removes notes at 1/64 resolution", "[auto]") {
    core::AutoSequencer seq;
    seq.setNotesForSlot(0, {});
    REQUIRE(seq.notesForSlot(0).empty());

    REQUIRE(seq.toggleNoteAt(0, 63, 1, 0, 1));
    REQUIRE(seq.notesForSlot(0).size() == 1u);
    REQUIRE(seq.notesForSlot(0).front().step64 == 63);
    REQUIRE(seq.notesForSlot(0).front().semitoneOffset == 0);

    REQUIRE(seq.toggleNoteAt(0, 63, 1, 7, 1));
    REQUIRE(seq.notesForSlot(0).size() == 1u);
    REQUIRE(seq.notesForSlot(0).front().semitoneOffset == 7);

    REQUIRE(!seq.toggleNoteAt(0, 63, 1, 7, 1));
    REQUIRE(seq.notesForSlot(0).empty());
}
