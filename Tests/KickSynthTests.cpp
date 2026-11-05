#include "KickSynth.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All 12 chromatic notes tune to exact equal-temperament fundamentals", "[synth]") {
    const float c1 = core::fundamentalHzForNote(0, 1, 0.0f);
    const float a1 = core::fundamentalHzForNote(9, 1, 0.0f);
    const float b1 = core::fundamentalHzForNote(11, 1, 0.0f);
    const float a2 = core::fundamentalHzForNote(9, 2, 0.0f);

    REQUIRE_THAT(a1, WithinAbs(55.0f, 0.001f));
    REQUIRE_THAT(a2, WithinAbs(110.0f, 0.001f));
    REQUIRE(c1 > 32.6f);
    REQUIRE(b1 < 61.8f);

    std::ranges::for_each(std::views::iota(0, core::kNumNotes - 1), [](int note) {
        const float lower = core::fundamentalHzForNote(note, 1, 0.0f);
        const float upper = core::fundamentalHzForNote(note + 1, 1, 0.0f);
        REQUIRE_THAT(upper / lower, WithinAbs(std::exp2(1.0f / 12.0f), 0.0001f));
    });
}

TEST_CASE("Locked-in WAV cache encodes valid RIFF WAV and isolates playback from synth edits", "[synth]") {
    const core::KickSynthSettings original{ 5, 1, 0.0f, 420.0f, 42.0f, 0.25f, 200.0f, 0.15f, 0.25f, 0.35f, 2800.0f, 0.45f };
    core::KickWavCache cache;
    cache.lockIn(original, 48000.0);

    REQUIRE(cache.isLocked());
    REQUIRE(cache.samples().size() == 9600u);
    REQUIRE(cache.wavBytes().size() == 44u + 9600u * sizeof(float));

    core::KickSynthSettings mutated = original;
    mutated.note = 11;
    mutated.startPitchHz = 1800.0f;
    mutated.tailLengthMs = 50.0f;
    mutated.clickLevel = 1.0f;

    core::KickVoice lockedVoice;
    lockedVoice.triggerForward(original, cache, 48000.0, 1.0f, 0.0f, -1.0f);
    REQUIRE(lockedVoice.isUsingCachedWav());

    const float firstLockedSample = lockedVoice.nextSample(mutated, cache, 48000.0);
    const float tenthLockedSample = [&] {
        float val = 0.0f;
        std::ranges::for_each(std::views::iota(0, 9), [&](int) {
            val = lockedVoice.nextSample(mutated, cache, 48000.0);
        });
        return val;
    }();

    REQUIRE_THAT(firstLockedSample, WithinAbs(cache.samples()[0], 0.0001f));
    REQUIRE_THAT(tenthLockedSample, WithinAbs(cache.samples()[9], 0.0001f));
}

TEST_CASE("Encoded WAV bytes decode back into identical kick samples", "[synth]") {
    const core::KickSynthSettings settings{};
    const auto rendered = core::renderKickWaveform(settings, 44100.0, 0.0f);
    const auto wavBytes = core::encodeWavBytes(rendered, 44100u);

    std::vector<float> decoded;
    uint32_t decodedRate = 0u;
    REQUIRE(core::decodeWavBytes(wavBytes, decoded, decodedRate));
    REQUIRE(decodedRate == 44100u);
    REQUIRE(decoded.size() == rendered.size());
    REQUIRE_THAT(decoded[100], WithinAbs(rendered[100], 1e-6f));
}
