#include "Effects.h"
#include "KickEngine.h"
#include "KickSynth.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
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
    const core::KickSettings synth{};
    core::KickVoice voice;
    voice.prepare(kSampleRate);
    voice.startReverse(synth, 60, 1.0f, fullBeat4Samples);

    float earlyEnergy = 0.0f;
    float lateEnergy = 0.0f;
    std::ranges::for_each(std::views::iota(0, fullBeat4Samples), [&](int i) {
        REQUIRE(voice.isActive());
        const float s = std::abs(voice.nextSample());
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

TEST_CASE("Beat synced reverse ends on the target sample across clocks and block sizes", "[effects][reverse]") {
    const auto expectSamples = [](core::ReverseSyncMode mode, double ppq, double bpm, double beats, double rate) {
        const double target = core::reverseTargetPpq(mode, ppq, beats);
        const double ppqPerSample = bpm / (60.0 * rate);
        const double exact = (target - ppq) / ppqPerSample;
        return std::max(0, static_cast<int>(std::ceil(exact - 1.0e-9)));
    };

    REQUIRE(core::reverseTargetPpq(core::ReverseSyncMode::Beat4Auto, 3.2, 4.0) == 4.0);
    REQUIRE(core::reverseTargetPpq(core::ReverseSyncMode::BeatEnd, 1.2, 4.0) == 2.0);
    REQUIRE(core::reverseTargetPpq(core::ReverseSyncMode::HalfBeat, 1.2, 4.0) == 1.5);
    REQUIRE(core::reverseTargetPpq(core::ReverseSyncMode::QuarterBeat, 1.2, 4.0) == 1.25);
    REQUIRE(core::reverseTargetPpq(core::ReverseSyncMode::Beat4Auto, 3.1, 3.5) == 3.5);
    REQUIRE(core::shouldTriggerReverse(core::ReverseSyncMode::Beat4Auto, false, 3.1, 3.5));

    const auto walk = [&](core::ReverseSyncMode mode, double startPpq, double bpm, double beats, double rate, int blockSize) {
        core::KickEngine engine;
        engine.prepare(rate);
        core::EngineSettings settings{};
        settings.effects.reverseMode = mode;
        settings.effects.reverseMix = 1.0f;
        settings.effects.drive = 0.0f;
        settings.effects.limiterEnabled = false;
        settings.synth.decayMs = 400.0f;
        const int expected = core::computeSyncedReverseSamples(mode, startPpq, bpm, beats, rate);
        if (expected != expectSamples(mode, startPpq, bpm, beats, rate) || expected <= 0) {
            return -1;
        }
        const double ppqPerSample = bpm / (60.0 * rate);
        int processed = 0;
        while (processed < expected) {
            const int count = std::min(blockSize, expected - processed);
            core::Transport transport{ bpm, startPpq + static_cast<double>(processed) * ppqPerSample, beats, true };
            std::vector<core::MidiTrigger> triggers;
            if (processed == 0) {
                triggers.push_back(core::MidiTrigger{ 0, 60, 1.0f, false });
            }
            std::vector<float> buffer(static_cast<size_t>(count), 0.0f);
            float* pointer = buffer.data();
            engine.process(settings, transport, triggers, &pointer, 1, count);
            processed += count;
            if (engine.isVoiceReversing() != (processed < expected)) {
                return -2;
            }
        }
        core::Transport boundary{ bpm, startPpq + static_cast<double>(expected) * ppqPerSample, beats, true };
        float sample = 0.0f;
        float* pointer = &sample;
        engine.process(settings, boundary, {}, &pointer, 1, 1);
        return engine.isVoiceReversing() ? -3 : expected;
    };

    const std::array<double, 3> rates{ 44100.0, 48000.0, 96000.0 };
    const std::array<double, 3> tempos{ 96.0, 128.5, 174.0 };
    const std::array<int, 3> blocks{ 1, 128, 513 };
    for (double rate : rates) {
        for (double bpm : tempos) {
            for (int block : blocks) {
                REQUIRE(walk(core::ReverseSyncMode::Beat4Auto, 3.0, bpm, 4.0, rate, block)
                    == core::computeSyncedReverseSamples(core::ReverseSyncMode::Beat4Auto, 3.0, bpm, 4.0, rate));
                REQUIRE(walk(core::ReverseSyncMode::BeatEnd, 0.37, bpm, 4.0, rate, block)
                    == core::computeSyncedReverseSamples(core::ReverseSyncMode::BeatEnd, 0.37, bpm, 4.0, rate));
                REQUIRE(walk(core::ReverseSyncMode::QuarterBeat, 2.05, bpm, 4.0, rate, block)
                    == core::computeSyncedReverseSamples(core::ReverseSyncMode::QuarterBeat, 2.05, bpm, 4.0, rate));
            }
        }
    }

    const double nearEnd = 4.0 - (5.0 * 90.0 / (60.0 * 48000.0));
    const int shortReverse = core::computeSyncedReverseSamples(
        core::ReverseSyncMode::BeatEnd, nearEnd, 90.0, 4.0, 48000.0);
    REQUIRE(shortReverse > 0);
    REQUIRE(shortReverse < 16);
    REQUIRE(walk(core::ReverseSyncMode::BeatEnd, nearEnd, 90.0, 4.0, 48000.0, 1) == shortReverse);

    core::KickEngine crossing;
    crossing.prepare(48000.0);
    core::EngineSettings crossingSettings{};
    crossingSettings.effects.reverseMode = core::ReverseSyncMode::Beat4Auto;
    crossingSettings.effects.reverseMix = 1.0f;
    crossingSettings.effects.drive = 0.0f;
    crossingSettings.effects.limiterEnabled = false;
    const int beatSamples = core::computeSyncedReverseSamples(core::ReverseSyncMode::Beat4Auto, 3.0, 120.0, 4.0, 48000.0);
    std::vector<float> longBlock(static_cast<size_t>(beatSamples + 300), 0.0f);
    float* longPtr = longBlock.data();
    const std::array<core::MidiTrigger, 1> downbeat{ { { 0, 60, 1.0f, false } } };
    crossing.process(crossingSettings, core::Transport{ 120.0, 3.0, 4.0, true }, downbeat, &longPtr, 1, beatSamples + 300);
    REQUIRE(!crossing.isVoiceReversing());

    core::KickEngine sought;
    sought.prepare(48000.0);
    float one = 0.0f;
    float* onePtr = &one;
    sought.process(crossingSettings, core::Transport{ 120.0, 3.0, 4.0, true }, downbeat, &onePtr, 1, 1);
    REQUIRE(sought.isVoiceReversing());
    sought.process(crossingSettings, core::Transport{ 120.0, 4.5, 4.0, true }, {}, &onePtr, 1, 1);
    REQUIRE(!sought.isVoiceReversing());

    core::KickEngine shifted;
    shifted.prepare(48000.0);
    double cursor = 3.0;
    int steps = 0;
    bool armed = false;
    while (steps < 100000) {
        const double bpm = steps < 2000 ? 100.0 : 170.0;
        float sample = 0.0f;
        float* pointer = &sample;
        std::vector<core::MidiTrigger> triggers;
        if (steps == 0) {
            triggers.push_back(core::MidiTrigger{ 0, 60, 1.0f, false });
        }
        shifted.process(crossingSettings, core::Transport{ bpm, cursor, 4.0, true }, triggers, &pointer, 1, 1);
        if (shifted.isVoiceReversing()) {
            armed = true;
        } else if (armed) {
            break;
        }
        cursor += bpm / (60.0 * 48000.0);
        ++steps;
    }
    double independent = 3.0;
    int independentSteps = 0;
    while (independent < 4.0 && independentSteps < 100000) {
        const double bpm = independentSteps < 2000 ? 100.0 : 170.0;
        independent += bpm / (60.0 * 48000.0);
        ++independentSteps;
    }
    REQUIRE(armed);
    REQUIRE(steps == independentSteps);
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

TEST_CASE("Bitcrusher quantizer stays on the bit grid and rejects NaNs", "[effects]") {
    core::BitCrusher crusher;
    std::ranges::for_each(std::views::iota(0, 240), [&](int i) {
        const float crushed = crusher.process(std::sin(static_cast<float>(i) * 0.17f), 3000.0f, 3, 48000.0);
        const float quantized = std::round(crushed * 4.0f) / 4.0f;
        REQUIRE(std::isfinite(crushed));
        REQUIRE_THAT(crushed, WithinAbs(quantized, 1.0e-5f));
    });

    crusher.reset();
    const float recovered = crusher.process(std::numeric_limits<float>::quiet_NaN(), 1000.0f, 4, 48000.0);
    REQUIRE(std::isfinite(recovered));
    REQUIRE_THAT(core::saturate(1.0f, core::Saturation::warm), WithinAbs(1.0f, 1.0e-4f));
    REQUIRE_THAT(core::saturate(std::numeric_limits<float>::quiet_NaN(), core::Saturation::tube), WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("Limiter catches the peak on the same sample and stays at the ceiling", "[effects]") {
    core::PeakLimiter limiter;
    limiter.prepare(48000.0);
    std::vector<float> buffer(128, 0.0f);
    buffer[0] = 2.0f;
    buffer[4] = std::numeric_limits<float>::quiet_NaN();
    buffer[20] = 0.25f;
    float* channel = buffer.data();
    limiter.process(&channel, 1, static_cast<int>(buffer.size()), 0.5f);

    REQUIRE_THAT(buffer[0], WithinAbs(0.5f, 1.0e-4f));
    std::ranges::for_each(buffer, [](float sample) {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 0.5001f);
    });
    REQUIRE(core::PeakLimiter::kLatency == 0);
}

TEST_CASE("Sync filter pump stays finite across a full sweep", "[effects]") {
    core::SyncFilterPump pump;
    std::ranges::for_each(std::views::iota(0, 8000), [&](int i) {
        const float input = 0.8f * std::sin(static_cast<float>(i) * 0.02f);
        const float filtered = pump.process(input, static_cast<double>(i) / 48000.0 * 2.0, 0.25, 1.0f, 44100.0);
        REQUIRE(std::isfinite(filtered));
        REQUIRE(std::abs(filtered) < 4.0f);
    });
    REQUIRE_THAT(pump.process(std::numeric_limits<float>::infinity(), 0.0, 0.25, 1.0f, 48000.0), WithinAbs(0.0f, 1.0e-6f));
}

namespace {

[[nodiscard]] double bandRms(const std::vector<float>& samples, double sampleRate, float cutoffHz, bool highBand) {
    float state = 0.0f;
    const float gain = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * static_cast<double>(cutoffHz) / sampleRate));
    double energy = 0.0;
    std::ranges::for_each(samples, [&](float sample) {
        state += gain * (sample - state);
        const float band = highBand ? sample - state : state;
        energy += static_cast<double>(band) * static_cast<double>(band);
    });
    return std::sqrt(energy / static_cast<double>(std::max<size_t>(1, samples.size())));
}

[[nodiscard]] std::vector<float> filteredTone(float frequencyHz, float highPassHz, float lowPassHz) {
    core::ToneFilter filter;
    std::vector<float> output(4800);
    std::ranges::for_each(std::views::iota(0, static_cast<int>(output.size())), [&](int i) {
        const float input = std::sin(2.0f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / 48000.0f);
        output[static_cast<size_t>(i)] = filter.process(input, highPassHz, lowPassHz, 48000.0);
    });
    return output;
}

} // namespace

TEST_CASE("High pass and low pass bypass exactly and cut the expected band", "[effects][filter]") {
    core::ToneFilter filter;
    std::ranges::for_each(std::views::iota(0, 512), [&](int i) {
        const float input = 0.25f * std::sin(static_cast<float>(i) * 0.2f);
        const float output = filter.process(input, core::ToneFilter::kHighPassBypassHz, core::ToneFilter::kLowPassBypassHz, 48000.0);
        REQUIRE(output == input);
    });

    const auto lowDry = filteredTone(80.0f, core::ToneFilter::kHighPassBypassHz, core::ToneFilter::kLowPassBypassHz);
    const auto lowHp = filteredTone(80.0f, 800.0f, core::ToneFilter::kLowPassBypassHz);
    const auto highDry = filteredTone(6000.0f, core::ToneFilter::kHighPassBypassHz, core::ToneFilter::kLowPassBypassHz);
    const auto highLp = filteredTone(6000.0f, core::ToneFilter::kHighPassBypassHz, 500.0f);
    const int settled = 800;
    REQUIRE(bandRms(lowHp, 48000.0, 200.0f, false) < bandRms(lowDry, 48000.0, 200.0f, false) * 0.35);
    REQUIRE(bandRms(highLp, 48000.0, 1500.0f, true) < bandRms(highDry, 48000.0, 1500.0f, true) * 0.45);
    (void)settled;

    std::ranges::for_each(std::array{ 8000.0, 44100.0, 96000.0, 192000.0 }, [&](double rate) {
        core::ToneFilter swept;
        float cutoff = 20.0f;
        std::ranges::for_each(std::views::iota(0, 2000), [&](int i) {
            cutoff = 20.0f + static_cast<float>(i) * 8.0f;
            const float lowPass = 20000.0f - static_cast<float>(i) * 6.0f;
            const float input = (i % 64 == 0) ? std::numeric_limits<float>::quiet_NaN() : std::sin(static_cast<float>(i) * 0.13f);
            const float output = swept.process(input, cutoff, lowPass, rate);
            REQUIRE(std::isfinite(output));
            REQUIRE(std::abs(output) < 4.0f);
        });
    });
}
