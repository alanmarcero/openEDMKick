#include "ClickSample.h"
#include "KickEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace core = openedmkick::core;

namespace {
float peakOf(const std::vector<float>& samples) {
    float peak = 0.0f;
    for (float sample : samples) peak = std::max(peak, std::abs(sample));
    return peak;
}

std::vector<float> renderPlayer(core::ClickSamplePlayer& player, int count) {
    std::vector<float> samples(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) samples[static_cast<size_t>(index)] = player.nextSample();
    return samples;
}
} // namespace

TEST_CASE("Built-in clicks are short deterministic sounds", "[click-sample]") {
    core::warmBuiltInClicks();
    std::vector<std::vector<float>> rendered;
    for (int index = 0; index < 4; ++index) {
        const auto& click = core::builtInClick(static_cast<core::BuiltInClick>(index));
        REQUIRE(click.sampleRate == 44100.0);
        REQUIRE(click.samples.size() > 32);
        REQUIRE(click.samples.size() < 44100 / 20);
        REQUIRE(peakOf(click.samples) > 0.4f);
        REQUIRE(peakOf(click.samples) <= 1.0f);
        for (float sample : click.samples) REQUIRE(std::isfinite(sample));
        const auto& again = core::builtInClick(static_cast<core::BuiltInClick>(index));
        REQUIRE(again.samples == click.samples);
        rendered.push_back(click.samples);
    }
    REQUIRE(rendered[0] != rendered[1]);
    REQUIRE(rendered[2] != rendered[3]);
}

TEST_CASE("Sample click pitch, start, level, and high-pass change playback", "[click-sample]") {
    std::vector<float> sine(4000);
    for (size_t index = 0; index < sine.size(); ++index)
        sine[index] = std::sin(2.0f * std::numbers::pi_v<float> * 200.0f * static_cast<float>(index) / 44100.0f);
    core::ClickSamplePlayer player;
    player.prepare(44100.0);
    player.setSource(sine.data(), static_cast<int>(sine.size()), 44100.0);

    player.trigger(0.0f, 0.0f, 0.0f, 20.0f);
    REQUIRE_FALSE(player.isActive());

    player.trigger(0.0f, 0.0f, 1.0f, 20.0f);
    int lowPitch = 0;
    float played = 0.0f;
    while (player.isActive() && lowPitch < 20000) {
        played += player.nextSample();
        ++lowPitch;
    }
    player.trigger(0.0f, 12.0f, 1.0f, 20.0f);
    int highPitch = 0;
    while (player.isActive() && highPitch < 20000) {
        played += player.nextSample();
        ++highPitch;
    }
    REQUIRE(highPitch < lowPitch * 0.7f);
    REQUIRE(lowPitch > 3000);

    player.trigger(0.5f, 0.0f, 1.0f, 20.0f);
    int trimmed = 0;
    while (player.isActive() && trimmed < 20000) {
        played += player.nextSample();
        ++trimmed;
    }
    REQUIRE(std::abs(played) > 0.0f);
    REQUIRE(trimmed < lowPitch * 0.7f);

    player.trigger(0.0f, 0.0f, 1.0f, 20.0f);
    const auto open = renderPlayer(player, 2000);
    player.trigger(0.0f, 0.0f, 1.0f, 8000.0f);
    const auto filtered = renderPlayer(player, 2000);
    const double openEnergy = std::inner_product(open.begin(), open.end(), open.begin(), 0.0);
    const double filteredEnergy = std::inner_product(filtered.begin(), filtered.end(), filtered.begin(), 0.0);
    REQUIRE(filteredEnergy < openEnergy * 0.55);
}

TEST_CASE("A disabled sample click does not change the engine", "[click-sample][engine]") {
    const auto render = [](bool enabled) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        const auto& click = core::builtInClick(core::BuiltInClick::snap);
        engine.setClickSampleSource(click.samples.data(), static_cast<int>(click.samples.size()), click.sampleRate);
        core::EngineSettings settings;
        settings.effects.limiterEnabled = false;
        settings.clickSample.enabled = enabled;
        settings.clickSample.level = 1.0f;
        settings.clickSample.highPassHz = 20.0f;
        const core::Transport transport{ 128.0, 0.0, 4.0, true };
        const std::array<core::MidiTrigger, 1> trigger{{ { 0, 36, 1.0f } }};
        std::vector<float> samples(2048);
        float* pointer = samples.data();
        engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(samples.size()));
        return samples;
    };
    const auto dry = render(false);
    const auto plain = [] {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings;
        settings.effects.limiterEnabled = false;
        const core::Transport transport{ 128.0, 0.0, 4.0, true };
        const std::array<core::MidiTrigger, 1> trigger{{ { 0, 36, 1.0f } }};
        std::vector<float> samples(2048);
        float* pointer = samples.data();
        engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(samples.size()));
        return samples;
    }();
    REQUIRE(dry == plain);
    const auto clicked = render(true);
    const double difference = std::inner_product(dry.begin(), dry.end(), clicked.begin(), 0.0, std::plus<>(),
        [](float left, float right) { return std::abs(static_cast<double>(left - right)); });
    REQUIRE(difference > 0.05);
}
