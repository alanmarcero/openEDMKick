#include "KickSynth.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numeric>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All chromatic notes tune to exact equal-temperament fundamentals", "[synth]") {
    REQUIRE_THAT(core::noteFrequencyHz(69), WithinAbs(440.0f, 0.001f));
    REQUIRE_THAT(core::noteFrequencyHz(57), WithinAbs(220.0f, 0.001f));
    REQUIRE_THAT(core::noteFrequencyHz(45), WithinAbs(110.0f, 0.001f));

    std::ranges::for_each(std::views::iota(60, 71), [](int note) {
        const float lower = core::noteFrequencyHz(note);
        const float upper = core::noteFrequencyHz(note + 1);
        REQUIRE_THAT(upper / lower, WithinAbs(std::exp2(1.0f / 12.0f), 0.0001f));
    });
}

TEST_CASE("Pitch sweep starts above the note and lands on the fundamental", "[synth]") {
    core::KickSettings settings{};
    REQUIRE(core::pitchOctavesAt(0.0f, settings) > 4.0f);
    REQUIRE(core::pitchOctavesAt(500.0f, settings) < 0.001f);

    settings.sweepOctaves = 0.0f;
    settings.snapOctaves = 0.0f;
    REQUIRE_THAT(core::pitchOctavesAt(0.0f, settings), WithinAbs(0.0f, 1.0e-6f));
    REQUIRE_THAT(core::pitchOctavesAt(500.0f, settings), WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("Live voice matches the rendered kick", "[synth]") {
    const core::KickSettings settings{};
    constexpr double rate = 48000.0;
    constexpr int midiNote = 69;
    const auto rendered = core::renderKick(settings, rate, midiNote);
    core::KickVoice voice;
    voice.prepare(rate);
    voice.start(settings, midiNote, 1.0f);

    std::ranges::for_each(std::views::iota(size_t{ 0 }, rendered.size()), [&](size_t index) {
        REQUIRE(voice.isActive());
        REQUIRE_THAT(voice.nextSample(), WithinAbs(rendered[index], 1.0e-6f));
    });
    REQUIRE_FALSE(voice.isActive());
    REQUIRE(!rendered.empty());
}

TEST_CASE("Positive decay curves fall faster than negative curves", "[synth]") {
    core::KickSettings fast{};
    fast.decayShape = 1.0f;
    core::KickSettings slow = fast;
    slow.decayShape = -1.0f;
    REQUIRE(core::amplitudeAt(180.0f, fast) < core::amplitudeAt(180.0f, slow));

    fast.sweepShape = 1.0f;
    slow.sweepShape = -1.0f;
    REQUIRE(core::pitchOctavesAt(35.0f, fast) < core::pitchOctavesAt(35.0f, slow));
}

TEST_CASE("Kick synthesis stays finite at parameter and sample-rate extremes", "[synth]") {
    const std::array<double, 4> rates{ 8000.0, 44100.0, 96000.0, 192000.0 };
    core::KickSettings settings{};
    settings.sweepOctaves = 6.0f;
    settings.snapOctaves = 5.0f;
    settings.drive = 1.0f;
    settings.clickLevel = 1.0f;
    settings.clickHz = 14000.0f;
    settings.decayMs = 1200.0f;
    settings.subLevel = 1.0f;
    settings.clip = 1.0f;

    std::ranges::for_each(rates, [&](double rate) {
        const auto buffer = core::renderKick(settings, rate, 93);
        REQUIRE(!buffer.empty());
        std::ranges::for_each(buffer, [](float sample) {
            REQUIRE(std::isfinite(sample));
            REQUIRE(sample <= 1.0f);
            REQUIRE(sample >= -1.0f);
        });
    });
}
