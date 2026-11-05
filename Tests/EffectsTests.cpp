#include "Effects.h"
#include "KickSynth.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("Beat 4 reverse effect finishes at the exact end of Beat 4", "[effects]") {
    constexpr double kBpm = 120.0;
    constexpr double kSampleRate = 48000.0;
    constexpr double kBeatsPerBar = 4.0;
    constexpr int kSamplesPerBeat = 24000;

    // Beat 4 in 0-indexed PPQ is [3.0, 4.0). Beats 1..3 are [0.0, 3.0).
    REQUIRE(!core::shouldTriggerReverse(core::ReverseSyncMode::Beat4Auto, false, 0.0, kBeatsPerBar));
    REQUIRE(!core::shouldTriggerReverse(core::ReverseSyncMode::Beat4Auto, false, 1.0, kBeatsPerBar));
    REQUIRE(!core::shouldTriggerReverse(core::ReverseSyncMode::Beat4Auto, false, 2.0, kBeatsPerBar));
    REQUIRE(core::shouldTriggerReverse(core::ReverseSyncMode::Beat4Auto, false, 3.0, kBeatsPerBar));

    // Triggered at start of Beat 4 (ppq = 3.0): must last exactly 24000 samples to end of Beat 4 (ppq = 4.0).
    const int fullBeat4Samples = core::computeSyncedReverseSamples(
        core::ReverseSyncMode::Beat4Auto, 3.0, kBpm, kBeatsPerBar, kSampleRate);
    REQUIRE(fullBeat4Samples == kSamplesPerBeat);

    // Triggered halfway through Beat 4 (ppq = 3.5): must last 12000 samples so it STILL finishes at end of Beat 4 (ppq = 4.0).
    const int halfBeat4Samples = core::computeSyncedReverseSamples(
        core::ReverseSyncMode::Beat4Auto, 3.5, kBpm, kBeatsPerBar, kSampleRate);
    REQUIRE(halfBeat4Samples == kSamplesPerBeat / 2);

    // Verify KickVoice reverse playback stays active for exactly fullBeat4Samples and finishes at sample 24000.
    const core::KickSynthSettings synth{};
    const core::KickWavCache cache{};
    core::KickVoice voice;
    voice.triggerReverse(synth, cache, kSampleRate, fullBeat4Samples, 1.0f, 0.0f);

    float earlyEnergy = 0.0f;
    float lateEnergy = 0.0f;
    std::ranges::for_each(std::views::iota(0, fullBeat4Samples), [&](int i) {
        REQUIRE(voice.isActive());
        const float s = std::abs(voice.nextSample(synth, cache, kSampleRate));
        if (i < 4000) {
            earlyEnergy += s;
        }
        if (i >= fullBeat4Samples - 4000) {
            lateEnergy += s;
        }
    });
    REQUIRE(!voice.isActive());
    REQUIRE(lateEnergy > earlyEnergy * 4.0f);
}

TEST_CASE("Bitcrusher quantizes amplitude and sample rate", "[effects]") {
    core::BitCrusher crusher;
    float diffSum = 0.0f;
    std::ranges::for_each(std::views::iota(0, 400), [&](int i) {
        const float clean = std::sin(static_cast<float>(i) * 0.09f);
        const float crushed = crusher.process(clean, 2000.0f, 3, 48000.0);
        diffSum += std::abs(clean - crushed);
    });
    REQUIRE(diffSum > 15.0f);
}
