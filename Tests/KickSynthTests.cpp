#include "KickEngine.h"
#include "KickSynth.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numeric>
#include <ranges>
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
    settings.clickLevel = 1.0f;
    settings.clickHz = 14000.0f;
    settings.decayMs = 1200.0f;
    settings.subLevel = 1.0f;

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

TEST_CASE("Kick, snare, clap, and hat layers render and filter independently", "[synth][layers]") {
    core::KickSettings settings{};
    settings.sweepOctaves = 0.0f;
    settings.snapOctaves = 0.0f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;
    settings.subLevel = 0.0f;
    settings.clickLevel = 0.0f;

    std::array<core::DrumLayerSettings, core::kNumSoundLayers> layers{};
    std::ranges::for_each(layers, [](core::DrumLayerSettings& layer) { layer.enabled = false; });
    std::array<std::vector<float>, 4> sounds;
    std::ranges::for_each(std::views::iota(0, 4), [&](int type) {
        layers[0] = { true, static_cast<core::DrumSoundType>(type), 1.0f,
                      type == 1 ? 12.0f : 0.0f, 180.0f };
        sounds[static_cast<size_t>(type)] = core::renderLayeredKick(settings, layers, 48000.0, 36);
        REQUIRE(!sounds[static_cast<size_t>(type)].empty());
        const double energy = std::accumulate(sounds[static_cast<size_t>(type)].begin(),
                                              sounds[static_cast<size_t>(type)].end(), 0.0,
            [](double sum, float sample) { return sum + std::abs(static_cast<double>(sample)); });
        REQUIRE(energy > 1.0);
    });

    const auto difference = [](const std::vector<float>& a, const std::vector<float>& b) {
        const size_t count = std::min(a.size(), b.size());
        return std::inner_product(a.begin(), a.begin() + static_cast<ptrdiff_t>(count), b.begin(), 0.0,
            std::plus<>(), [](float left, float right) { return std::abs(static_cast<double>(left - right)); });
    };
    REQUIRE(difference(sounds[0], sounds[1]) > 10.0);
    REQUIRE(difference(sounds[1], sounds[2]) > 10.0);
    REQUIRE(difference(sounds[2], sounds[3]) > 10.0);

    // Drum layers (snare, clap, hat) must add substantial audible energy on top of a kick
    for (int extra = 1; extra <= 3; ++extra) {
        std::array<core::DrumLayerSettings, core::kNumSoundLayers> baseLayers{};
        baseLayers[0] = { true, core::DrumSoundType::kick, 1.0f, 0.0f, 180.0f };
        auto layered = baseLayers;
        layered[1] = { true, static_cast<core::DrumSoundType>(extra), 0.7f, 0.0f, 120.0f };
        const auto kickOnly = core::renderLayeredKick(settings, baseLayers, 48000.0, 36);
        const auto withLayer = core::renderLayeredKick(settings, layered, 48000.0, 36);
        REQUIRE(difference(kickOnly, withLayer) > 40.0);
    }

    settings.sweepOctaves = settings.snapOctaves = 0.0f;
    settings.clickLevel = settings.subLevel = 0.0f;
    layers[0] = { true, core::DrumSoundType::kick, 1.0f, 0.0f, 300.0f };
    const auto dry = core::renderLayeredKick(settings, layers, 48000.0, 36);
    layers[0].highPassHz = 1500.0f;
    const auto filtered = core::renderLayeredKick(settings, layers, 48000.0, 36);
    const double dryEnergy = std::accumulate(dry.begin(), dry.end(), 0.0,
        [](double sum, float sample) { return sum + static_cast<double>(sample) * sample; });
    const double filteredEnergy = std::accumulate(filtered.begin(), filtered.end(), 0.0,
        [](double sum, float sample) { return sum + static_cast<double>(sample) * sample; });
    REQUIRE(filteredEnergy < dryEnergy * 0.35);
}

namespace {
double bodyRms(const core::KickSettings& settings, double rate, int note) {
    core::KickVoice voice;
    voice.prepare(rate);
    voice.start(settings, note, 1);
    const int start = static_cast<int>(rate * 0.10);
    const int end = static_cast<int>(rate * 0.18);
    double energy = 0;
    for (int i = 0; i < end; ++i) {
        const double sample = voice.nextSample();
        if (i >= start) energy += sample * sample;
    }
    return std::sqrt(energy / (end - start));
}
}

TEST_CASE("The sub reinforces the settled kick across pitches and gate modes", "[synth][sub]") {
    core::KickSettings settings;
    settings.sweepMs = 42;
    settings.bodyTone = 0.24f;
    settings.holdMs = 220;
    settings.knockDb = 0;
    settings.ampDecayMs = 10;
    settings.ampSustain = 0.8f;
    for (double rate : {44100., 96000.}) {
        for (bool gated : {false, true}) {
            settings.ampEnabled = gated;
            for (int note : {29, 33, 36, 41, 45, 48}) {
                settings.subLevel = 0;
                const double body = bodyRms(settings, rate, note);
                settings.subLevel = 1;
                CAPTURE(rate, gated, note);
                CHECK(bodyRms(settings, rate, note) > 0.93 * body);
            }
        }
    }
}

TEST_CASE("Knock emphasizes the selected frequency while preserving the distant sub", "[synth][knock]") {
    core::KickSettings settings;
    settings.sweepOctaves = settings.snapOctaves = settings.clickLevel = 0;
    settings.bodyTone = settings.subLevel = 0;
    settings.holdMs = 220;
    for (double rate : {44100., 96000.}) {
        settings.knockDb = 0;
        const double dry = bodyRms(settings, rate, core::kDefaultMidiNote);
        settings.knockDb = 12;
        settings.knockHz = 800;
        CHECK(bodyRms(settings, rate, core::kDefaultMidiNote) < dry * 1.05);
        settings.knockHz = core::noteFrequencyHz(core::kDefaultMidiNote);
        CHECK(bodyRms(settings, rate, core::kDefaultMidiNote) > dry * 1.15);
    }
}

namespace {

double goertzel(const std::vector<float>& samples, size_t start, size_t count, double hz, double rate) {
    const double coeff = 2.0 * std::cos(2.0 * std::numbers::pi * hz / rate);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < count && start + i < samples.size(); ++i) {
        const double s0 = samples[start + i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2));
}

} // namespace

TEST_CASE("Kick harmonics fall away after the pitch dive", "[synth][quality]") {
    core::KickSettings settings{};
    settings.bodyTone = 1.0f;
    settings.sweepOctaves = 2.0f;
    settings.sweepMs = 24.0f;
    settings.snapOctaves = 1.0f;
    settings.clickLevel = 0.0f;
    settings.subLevel = 0.7f;
    settings.holdMs = 4.0f;
    settings.decayMs = 180.0f;
    settings.knockDb = 0.0f;
    const auto hit = core::renderKick(settings, 48000.0, 36);
    const double fundamental = core::noteFrequencyHz(36);
    const double early = goertzel(hit, static_cast<size_t>(48000 * 0.004), 512, fundamental * 3.0, 48000.0);
    const double late = goertzel(hit, static_cast<size_t>(48000 * 0.08), 2048, fundamental * 3.0, 48000.0);
    const double lateFund = goertzel(hit, static_cast<size_t>(48000 * 0.08), 2048, fundamental, 48000.0);
    CHECK(late < early * 0.35);
    CHECK(late < lateFund * 0.15);
    for (float sample : hit) {
        CHECK(std::isfinite(sample));
        CHECK(std::abs(sample) <= 1.0f);
    }
}

TEST_CASE("Retriggering a sustaining kick does not jump", "[synth][quality]") {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.synth.sweepOctaves = 0.4f;
    settings.synth.snapOctaves = 0.1f;
    settings.synth.clickLevel = 0.0f;
    settings.synth.bodyTone = 0.1f;
    settings.synth.holdMs = 8.0f;
    settings.synth.decayMs = 400.0f;
    settings.synth.subLevel = 0.8f;
    settings.effects.limiterEnabled = false;
    settings.layers[1].enabled = settings.layers[2].enabled = settings.layers[3].enabled = false;
    const int at = static_cast<int>(48000 * 0.08);
    std::vector<float> left(static_cast<size_t>(48000 * 0.16));
    float* channel = left.data();
    const std::array<core::MidiTrigger, 2> notes{{ { 0, 36, 1.0f, false, 1 }, { at, 36, 1.0f, false, 1 } }};
    engine.process(settings, { 128.0, 0.0, 4.0, true }, notes, &channel, 1, static_cast<int>(left.size()));
    float step = 0.0f;
    for (int i = at; i < at + 30; ++i)
        step = std::max(step, std::abs(left[static_cast<size_t>(i)] - left[static_cast<size_t>(i - 1)]));
    CHECK(step < 0.05f);
}

TEST_CASE("Hat partials do not ring through the noise decay", "[synth][layers][quality]") {
    core::KickSettings settings{};
    settings.clickLevel = settings.subLevel = settings.bodyTone = 0.0f;
    core::DrumLayerSettings hat{};
    hat.enabled = true;
    hat.sound = core::DrumSoundType::hat;
    hat.gain = 1.0f;
    hat.decayMs = 80.0f;
    hat.highPassHz = 4000.0f;
    hat.lowPassHz = 16000.0f;
    std::array<core::DrumLayerSettings, core::kNumSoundLayers> layers{};
    layers[0] = hat;
    const auto hit = core::renderLayeredKick(settings, layers, 48000.0, 36);
    const double early = goertzel(hit, 0, 400, 6500.0, 48000.0);
    const double late = goertzel(hit, static_cast<size_t>(48000 * 0.03), 600, 6500.0, 48000.0);
    CHECK(late < early * 0.45);
}

namespace {

double complexPhaseAt(const std::vector<float>& samples, size_t start, size_t count, double hz, double rate) {
    double real = 0.0;
    double imag = 0.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, count), [&](size_t i) {
        if (start + i >= samples.size()) return;
        const double angle = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / rate;
        real += static_cast<double>(samples[start + i]) * std::cos(angle);
        imag -= static_cast<double>(samples[start + i]) * std::sin(angle);
    });
    return std::atan2(imag, real);
}

double windowRms(const std::vector<float>& samples, size_t start, size_t end) {
    const size_t clampedEnd = std::min(end, samples.size());
    if (clampedEnd <= start) return 0.0;
    double sumSq = 0.0;
    std::ranges::for_each(std::views::iota(start, clampedEnd), [&](size_t i) {
        const double v = samples[i];
        sumSq += v * v;
    });
    return std::sqrt(sumSq / static_cast<double>(clampedEnd - start));
}

} // namespace

TEST_CASE("Analytic pitch integration keeps fast snap sweeps phase-accurate across rates", "[synth][dsp]") {
    core::KickSettings settings{};
    settings.attackMs = 0.05f;
    settings.holdMs = 80.0f;
    settings.decayMs = 120.0f;
    settings.sweepOctaves = 4.5f;
    settings.sweepMs = 18.0f;
    settings.snapOctaves = 5.0f;
    settings.snapMs = 0.25f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;
    settings.subLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.lowCutHz = 10.0f;
    settings.toneHz = 20000.0f;

    // 64 kHz runs 2x with an 11.5-sample decimator delay; 192 kHz runs 1x with zero delay.
    const auto hit64 = core::renderKick(settings, 64000.0, 36);
    const auto hit192 = core::renderKick(settings, 192000.0, 36);
    const double hz = core::noteFrequencyHz(36);
    const double decimatorDelaySec = 11.5 / 64000.0;
    const double measureTimeSec = 0.035;
    const size_t start64 = static_cast<size_t>(std::lround((measureTimeSec + decimatorDelaySec) * 64000.0));
    const double exact64Time = static_cast<double>(start64) / 64000.0 - decimatorDelaySec;
    const size_t start192 = static_cast<size_t>(std::lround(exact64Time * 192000.0));
    const double phase64 = complexPhaseAt(hit64, start64, 1280, hz, 64000.0);
    const double phase192 = complexPhaseAt(hit192, start192, 3840, hz, 192000.0);
    const double timeDiff = static_cast<double>(start192) / 192000.0 - exact64Time;
    const double compensated192 = phase192 - 2.0 * std::numbers::pi * hz * timeDiff;
    const double wrappedDiff = std::abs(std::remainder(phase64 - compensated192, 2.0 * std::numbers::pi));
    REQUIRE(wrappedDiff < 0.08);
}

TEST_CASE("Phase-locked sub reinforces the body throughout the pitch sweep transition", "[synth][sub][dsp]") {
    core::KickSettings base{};
    base.sweepOctaves = 2.6f;
    base.snapOctaves = 1.4f;
    base.bodyTone = 0.1f;
    base.knockDb = 0.0f;
    base.clickLevel = 0.0f;
    base.holdMs = 120.0f;
    base.decayMs = 200.0f;

    const std::array<float, 6> sweepTimes{ 18.0f, 23.0f, 29.0f, 35.0f, 41.0f, 47.0f };
    std::ranges::for_each(sweepTimes, [&](float sweepMs) {
        auto noSub = base;
        noSub.sweepMs = sweepMs;
        noSub.subLevel = 0.0f;
        auto fullSub = noSub;
        fullSub.subLevel = 1.0f;

        const auto dry = core::renderKick(noSub, 48000.0, 36);
        const auto reinforced = core::renderKick(fullSub, 48000.0, 36);
        // Around 0.24..0.34 * sweepMs, pitchOctavesAt crosses ~0.25 octaves (50/50 sub crossfade zone).
        const size_t start = static_cast<size_t>(48.0f * sweepMs * 0.24f);
        const size_t end = static_cast<size_t>(48.0f * sweepMs * 0.36f);
        CAPTURE(sweepMs);
        REQUIRE(windowRms(reinforced, start, end) > windowRms(dry, start, end) * 1.15);
    });
}

TEST_CASE("Oversampled ADAA body saturation adds warm low-mid even harmonics on driven hits", "[synth][dsp]") {
    core::KickSettings settings{};
    settings.sweepOctaves = 0.0f;
    settings.snapOctaves = 0.0f;
    settings.bodyTone = 0.8f;
    settings.knockDb = 0.0f;
    settings.clickLevel = 0.0f;
    settings.subLevel = 1.0f;
    settings.clickPunch = 0.0f;
    settings.holdMs = 200.0f;
    settings.decayMs = 200.0f;
    settings.toneHz = 12000.0f;

    // A 4096-sample Hann window places 2*f0 5.6 bins above f0 so fundamental leakage stays below -70 dB.
    const auto hit = core::renderKick(settings, 48000.0, 36);
    const double f0 = core::noteFrequencyHz(36);
    const auto hannMag = [&](double hz) {
        double re = 0.0;
        double im = 0.0;
        constexpr size_t count = 4096;
        std::ranges::for_each(std::views::iota(size_t{ 0 }, count), [&](size_t i) {
            const double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(count - 1)));
            const double s = static_cast<double>(hit[2400 + i]) * w;
            const double a = 2.0 * std::numbers::pi * hz * static_cast<double>(i) / 48000.0;
            re += s * std::cos(a);
            im -= s * std::sin(a);
        });
        return std::hypot(re, im);
    };
    const double h1 = hannMag(f0);
    const double h2 = hannMag(f0 * 2.0);
    REQUIRE(h1 > 100.0);
    REQUIRE(h2 > h1 * 0.008);
}

TEST_CASE("ZDF SVF voice filters preserve steady DC state under fast cutoff modulation", "[synth][dsp]") {
    const double rate = 48000.0;
    openedmkick::dsp::Biquad filter;
    filter.c = openedmkick::dsp::lowpassCoeffs(200.0, 3.5, rate);
    std::ranges::for_each(std::views::iota(0, 4096), [&](int) {
        (void)filter.process(0.5f);
    });

    float maxDeviation = 0.0f;
    std::ranges::for_each(std::views::iota(0, 1024), [&](int n) {
        const double cutoff = 150.0 + 14850.0 * (0.5 + 0.5 * std::sin(0.31 * static_cast<double>(n)));
        filter.c = openedmkick::dsp::lowpassCoeffs(cutoff, 3.5, rate);
        const float y = filter.process(0.5f);
        REQUIRE(std::isfinite(y));
        maxDeviation = std::max(maxDeviation, std::abs(y - 0.5f));
    });
    REQUIRE(maxDeviation < 1.0e-4f);
}

TEST_CASE("Modal beater transient excites multiple inharmonic contact partials", "[synth][click][dsp]") {
    core::KickSettings settings{};
    settings.sweepOctaves = 0.0f;
    settings.snapOctaves = 0.0f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;
    settings.subLevel = 0.0f;
    settings.clickType = core::ClickType::beater;
    settings.clickLevel = 1.0f;
    settings.clickHz = 2000.0f;
    settings.clickMs = 16.0f;
    settings.attackMs = 0.05f;
    settings.holdMs = 20.0f;
    settings.decayMs = 20.0f;
    settings.lowCutHz = 20.0f;
    settings.toneHz = 20000.0f;

    const auto hit = core::renderKick(settings, 48000.0, 36);
    const double mode1 = goertzel(hit, 0, 768, 2000.0, 48000.0);
    const double valley = goertzel(hit, 0, 768, 2550.0, 48000.0);
    const double mode2 = goertzel(hit, 0, 768, 3186.0, 48000.0);
    const double mode3 = goertzel(hit, 0, 768, 4272.0, 48000.0);
    REQUIRE(mode1 > 0.2);
    REQUIRE(mode2 > valley * 1.45);
    REQUIRE(mode3 > mode1 * 0.12);
}

TEST_CASE("Reverse playback renders causal forward filters into cache and plays strictly in reverse", "[synth][reverse][dsp]") {
    core::KickSettings settings{};
    settings.sweepOctaves = 2.5f;
    settings.sweepMs = 45.0f;
    settings.knockHz = 175.0f;
    settings.knockDb = 9.0f;
    settings.toneHz = 4500.0f;
    settings.holdMs = 25.0f;
    settings.decayMs = 220.0f;

    const auto forward = core::renderKick(settings, 48000.0, 36);
    REQUIRE(forward.size() > 2000);

    core::KickVoice voice;
    voice.prepare(48000.0);
    voice.startReverse(settings, 36, 1.0f, static_cast<int>(forward.size()));
    std::vector<float> reversed(forward.size(), 0.0f);
    std::ranges::for_each(std::views::iota(size_t{ 0 }, forward.size()), [&](size_t i) {
        reversed[i] = voice.nextSample();
    });

    // After the raised-cosine tail fade-in and before the end declick, reversed matches exact time-reversed forward.
    const size_t checkStart = forward.size() / 3;
    const size_t checkEnd = forward.size() - 256;
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    std::ranges::for_each(std::views::iota(checkStart, checkEnd), [&](size_t i) {
        const double a = reversed[i];
        const double b = forward[forward.size() - 1 - i];
        dot += a * b;
        normA += a * a;
        normB += b * b;
    });
    const double correlation = dot / std::sqrt(std::max(1.0e-12, normA * normB));
    REQUIRE(correlation > 0.998);
    REQUIRE(std::abs(reversed.front()) < 1.0e-4f);
}

TEST_CASE("Snare layer synthesizes coupled dual-membrane modes and comb wire rattle", "[synth][layers][dsp]") {
    core::KickSettings settings{};
    settings.clickLevel = 0.0f;
    settings.subLevel = 0.0f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;

    std::array<core::DrumLayerSettings, core::kNumSoundLayers> layers{};
    std::ranges::for_each(layers, [](auto& l) { l.enabled = false; });
    layers[0] = { true, core::DrumSoundType::snare, 1.0f, 0.0f, 220.0f };

    const auto snare = core::renderLayeredKick(settings, layers, 48000.0, 36);
    const double modeLow = goertzel(snare, 48, 1024, 195.0, 48000.0);
    const double betweenModes = goertzel(snare, 48, 1024, 244.0, 48000.0);
    const double modeHigh = goertzel(snare, 48, 1024, 296.0, 48000.0);
    REQUIRE(modeLow > 1.0);
    REQUIRE(modeHigh > betweenModes * 1.45);
}

TEST_CASE("Hat layer synthesizes irrational square-ring sidebands without phase-wrap clicks", "[synth][layers][dsp]") {
    core::KickSettings settings{};
    settings.clickLevel = 0.0f;
    settings.subLevel = 0.0f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;
    settings.toneHz = 20000.0f;

    std::array<core::DrumLayerSettings, core::kNumSoundLayers> layers{};
    std::ranges::for_each(layers, [](auto& l) { l.enabled = false; });
    layers[0] = { true, core::DrumSoundType::hat, 1.0f, 0.0f, 160.0f };

    const auto hat = core::renderLayeredKick(settings, layers, 48000.0, 24);
    // At note 24, base hat carrier is 3250 Hz; ring-mod sum sideband of (1.0, 1.342) sits at 3250 * 2.342 = 7611.5 Hz.
    const double sumSideband = goertzel(hat, 0, 1024, 3250.0 * 2.342, 48000.0);
    const double floor = goertzel(hat, 0, 1024, 420.0, 48000.0);
    REQUIRE(sumSideband > floor * 2.2);
}
