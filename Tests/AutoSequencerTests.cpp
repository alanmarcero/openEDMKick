#include "AutoSequencer.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All 12 fill presets generate valid patterns across 1 to 64 bars", "[auto]") {
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
