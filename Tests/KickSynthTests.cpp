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

TEST_CASE("Kick, snare, clap, and hat layers render and filter independently", "[synth][layers]") {
    core::KickSettings settings{};
    settings.sweepOctaves = 0.0f;
    settings.snapOctaves = 0.0f;
    settings.bodyTone = 0.0f;
    settings.knockDb = 0.0f;
    settings.subLevel = 0.0f;
    settings.clickLevel = 0.0f;
    settings.drive = 0.0f;
    settings.clip = 0.0f;

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
    settings.drive = settings.clip = settings.clickLevel = settings.subLevel = 0.0f;
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
    settings.drive = 0.1f;
    settings.bodyTone = 0.24f;
    settings.holdMs = 220;
    settings.knockDb = settings.clip = 0;
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
    settings.drive = settings.bodyTone = settings.subLevel = settings.clip = 0;
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
