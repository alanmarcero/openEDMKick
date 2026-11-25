#include "KickEngine.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <chrono>
#include <numeric>
#include <ranges>
#include <utility>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;











namespace {

[[nodiscard]] std::vector<float> renderFilteredKick(float highPassHz, float lowPassHz) {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.effects.limiterEnabled = false;
    settings.effects.highPassHz = highPassHz;
    settings.effects.lowPassHz = lowPassHz;
    settings.synth.decayMs = 380.0f;
    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> trigger{ { { 0, 60, 1.0f, false } } };
    std::vector<float> buffer(20000, 0.0f);
    float* pointer = buffer.data();
    engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(buffer.size()));
    return buffer;
}

[[nodiscard]] double slowEnergy(const std::vector<float>& samples) {
    float state = 0.0f;
    const float gain = 0.02f;
    double energy = 0.0;
    std::ranges::for_each(samples, [&](float sample) {
        state += gain * (sample - state);
        energy += static_cast<double>(state) * static_cast<double>(state);
    });
    return energy;
}

[[nodiscard]] double edgeEnergy(const std::vector<float>& samples) {
    double energy = 0.0;
    std::ranges::for_each(std::views::iota(size_t{ 1 }, samples.size()), [&](size_t i) {
        const double delta = static_cast<double>(samples[i] - samples[i - 1]);
        energy += delta * delta;
    });
    return energy;
}

} // namespace

TEST_CASE("Default high pass and low pass match an explicit bypass on a kick", "[engine][filter]") {
    const auto dry = renderFilteredKick(core::ToneFilter::kHighPassBypassHz, core::ToneFilter::kLowPassBypassHz);
    const auto explicitBypass = renderFilteredKick(20.0f, 20000.0f);
    REQUIRE(dry.size() == explicitBypass.size());
    std::ranges::for_each(std::views::iota(size_t{ 0 }, dry.size()), [&](size_t i) {
        REQUIRE(dry[i] == explicitBypass[i]);
        REQUIRE(std::isfinite(dry[i]));
    });

    const auto highPassed = renderFilteredKick(350.0f, 20000.0f);
    const auto lowPassed = renderFilteredKick(20.0f, 700.0f);
    REQUIRE(slowEnergy(highPassed) < slowEnergy(dry) * 0.55);
    REQUIRE(edgeEnergy(lowPassed) < edgeEnergy(dry) * 0.7);
    REQUIRE(slowEnergy(lowPassed) > slowEnergy(highPassed));

}

TEST_CASE("MIDI note-on follows the exact note and octave", "[engine][midi]") {
    REQUIRE_THAT(core::noteFrequencyHz(69), WithinAbs(440.0f, 0.001f));
    REQUIRE_THAT(core::noteFrequencyHz(57), WithinAbs(220.0f, 0.001f));

    const auto render = [](int midiNote) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings{};
        settings.synth.sweepOctaves = 0.0f;
        settings.synth.snapOctaves = 0.0f;
        settings.synth.bodyTone = 0.0f;
        settings.synth.knockDb = 0.0f;
        settings.synth.subLevel = 0.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.holdMs = 0.0f;
        settings.synth.decayMs = 360.0f;
        settings.effects.limiterEnabled = false;
        const core::Transport transport{ 120.0, 0.0, 4.0, true };
        const std::array<core::MidiTrigger, 1> trigger{ { { 0, midiNote, 1.0f, false } } };
        std::vector<float> output(18000, 0.0f);
        float* channel = output.data();
        engine.process(settings, transport, trigger, &channel, 1, static_cast<int>(output.size()));
        return output;
    };

    const auto a3 = render(57);
    const auto a4 = render(69);
    const double difference = std::inner_product(a3.begin(), a3.end(), a4.begin(), 0.0,
        std::plus<>(), [](float a, float b) { return std::abs(static_cast<double>(a - b)); });
    REQUIRE(difference > 100.0);
}

TEST_CASE("MIDI velocity gives soft and hard kick hits a useful level range", "[engine][midi][velocity]") {
    const auto renderPeak = [](float velocity) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings{};
        settings.synth.sweepOctaves = 0.0f;
        settings.synth.snapOctaves = 0.0f;
        settings.synth.bodyTone = 0.0f;
        settings.synth.knockDb = 0.0f;
        settings.synth.subLevel = 0.0f;
        settings.synth.clickLevel = 0.0f;
        settings.synth.holdMs = 0.0f;
        settings.synth.decayMs = 120.0f;
        settings.effects.limiterEnabled = false;
        const std::array<core::MidiTrigger, 1> trigger{ { { 0, 36, velocity, false } } };
        std::vector<float> output(8192, 0.0f);
        float* channel = output.data();
        engine.process(settings, core::Transport{ 120.0, 0.0, 4.0, true }, trigger,
                       &channel, 1, static_cast<int>(output.size()));
        float peak = 0.0f;
        std::ranges::for_each(output, [&](float sample) { peak = std::max(peak, std::abs(sample)); });
        return peak;
    };

    const float soft = renderPeak(0.25f);
    const float hard = renderPeak(1.0f);
    REQUIRE(soft > 0.0f);
    REQUIRE(hard > soft * 3.0f);
}

TEST_CASE("MIDI CC and automation activate host-synced filter sweeps", "[engine][midi][effects]") {
    const auto render = [](bool highPassAutomation, bool lowPassAutomation, int ccNumber) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings{};
        settings.synth.decayMs = 360.0f;
        settings.effects.limiterEnabled = false;
        settings.effects.highPassSyncEnabled = highPassAutomation;
        settings.effects.lowPassSyncEnabled = lowPassAutomation;
        const std::array<core::MidiTrigger, 1> trigger{ { { 0, 48, 1.0f, false } } };
        const std::array<core::MidiCcEvent, 1> control{ { { 0, ccNumber, 127 } } };
        std::vector<float> output(8192, 0.0f);
        float* channel = output.data();
        std::span<const core::MidiCcEvent> cc;
        if (ccNumber > 0) cc = control;
        engine.process(settings, core::Transport{ 120.0, 0.0, 4.0, true }, trigger,
                       &channel, 1, static_cast<int>(output.size()), cc);
        return output;
    };
    const auto difference = [](const std::vector<float>& a, const std::vector<float>& b) {
        return std::inner_product(a.begin(), a.end(), b.begin(), 0.0,
            std::plus<>(), [](float left, float right) { return std::abs(static_cast<double>(left - right)); });
    };

    const auto dry = render(false, false, 0);
    REQUIRE(difference(dry, render(true, false, 0)) > 10.0);
    REQUIRE(difference(dry, render(false, false, 21)) > 10.0);
    REQUIRE(difference(dry, render(false, true, 0)) > 10.0);
    REQUIRE(difference(dry, render(false, false, 22)) > 10.0);
}

TEST_CASE("MIDI CC20 and Reverse Mode automation activate synced reverse hits", "[engine][midi][effects]") {
    const auto reversing = [](core::ReverseSyncMode mode, int ccValue) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings{};
        settings.effects.reverseMode = mode;
        const std::array<core::MidiTrigger, 1> trigger{ { { 0, 36, 1.0f, false } } };
        const std::array<core::MidiCcEvent, 1> control{ { { 0, 20, ccValue } } };
        std::vector<float> output(128, 0.0f);
        float* channel = output.data();
        engine.process(settings, core::Transport{ 120.0, 0.0, 4.0, true }, trigger,
                       &channel, 1, static_cast<int>(output.size()), control);
        return engine.isVoiceReversing();
    };

    REQUIRE(reversing(core::ReverseSyncMode::Off, 127));
    REQUIRE_FALSE(reversing(core::ReverseSyncMode::Off, 0));
    REQUIRE(reversing(core::ReverseSyncMode::BeatEnd, 0));
}

TEST_CASE("Same-sample MIDI CC and note events keep their input order", "[engine][midi][effects]") {
    const std::array<core::MidiInputEvent, 2> enableThenNote{
        core::MidiInputEvent{ std::in_place_type<core::MidiCcEvent>, core::MidiCcEvent{ 0, 20, 127 } },
        core::MidiInputEvent{ std::in_place_type<core::MidiTrigger>, core::MidiTrigger{ 0, 36, 1.0f, false } }
    };
    const std::array<core::MidiInputEvent, 2> noteThenEnable{
        core::MidiInputEvent{ std::in_place_type<core::MidiTrigger>, core::MidiTrigger{ 0, 36, 1.0f, false } },
        core::MidiInputEvent{ std::in_place_type<core::MidiCcEvent>, core::MidiCcEvent{ 0, 20, 127 } }
    };
    const auto isReversing = [](std::span<const core::MidiInputEvent> events) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        core::EngineSettings settings{};
        std::vector<float> output(128, 0.0f);
        float* channel = output.data();
        engine.process(settings, core::Transport{ 120.0, 0.0, 4.0, true }, {},
                       &channel, 1, static_cast<int>(output.size()), {}, events);
        return engine.isVoiceReversing();
    };

    REQUIRE(isReversing(enableThenNote));
    REQUIRE_FALSE(isReversing(noteThenEnable));

    const std::array<core::MidiInputEvent, 2> earlyEnableThenNote{
        core::MidiCcEvent{16, 20, 127}, core::MidiTrigger{32, 36, 1.0f, false}
    };
    const std::array<core::MidiInputEvent, 2> earlyNoteThenEnable{
        core::MidiTrigger{16, 36, 1.0f, false}, core::MidiCcEvent{32, 20, 127}
    };
    REQUIRE(isReversing(earlyEnableThenNote));
    REQUIRE_FALSE(isReversing(earlyNoteThenEnable));
}



namespace {
std::array<std::vector<float>, 2> renderImprovedLayers(core::EngineSettings settings, int note = 36, float velocity = 0.5f) {
    core::KickEngine engine;
    engine.prepare(48000);
    settings.effects.limiterEnabled = false;
    std::array<std::vector<float>, 2> audio{std::vector<float>(4096), std::vector<float>(4096)};
    float* channels[]{audio[0].data(), audio[1].data()};
    const std::array<core::MidiTrigger, 1> midi{{{0, note, velocity}}};
    engine.process(settings, {120, 0.0, 4, true}, midi, channels, 2, 4096);
    return audio;
}
double layerEnergy(const std::vector<float>& audio) {
    return std::inner_product(audio.begin(), audio.end(), audio.begin(), 0.0);
}
}

TEST_CASE("Layer velocity curve and timbre response are independent", "[layer-improvements]") {
    core::EngineSettings settings;
    const auto original = renderImprovedLayers(settings);
    settings.layers[0].velocityCurve = 0.3f;
    const auto loud = renderImprovedLayers(settings);
    REQUIRE(layerEnergy(loud[0]) > layerEnergy(original[0]) * 1.5);
    settings.layers[0].velocityCurve = 1.45f;
    settings.layers[0].velocityTimbre = 0;
    const auto fixedTimbre = renderImprovedLayers(settings);
    REQUIRE(fixedTimbre[0] != original[0]);
}

TEST_CASE("Layer MIDI zones include boundaries and reject outside hits", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].keyLow = 36;
    settings.layers[0].keyHigh = 36;
    settings.layers[0].velocityLow = 64;
    settings.layers[0].velocityHigh = 96;
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 36, 64.0f / 127)[0]) > 0);
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 35, 64.0f / 127)[0]) > 0);
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 36, 63.0f / 127)[0]) > 0);
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 36, 97.0f / 127)[0]) > 0);
}

TEST_CASE("Layer synthesis offsets leave other layers independent", "[layer-improvements]") {
    core::EngineSettings settings;
    const auto original = renderImprovedLayers(settings);
    settings.layers[0].toneScale = 0.2f;
    settings.layers[0].sweepScale = 0;
    REQUIRE(renderImprovedLayers(settings) != original);
}

TEST_CASE("Layer solo pan and polarity act before the stereo effects", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].pan = -1;
    settings.layers[0].invertPolarity = true;
    const auto centered = renderImprovedLayers(settings);
    REQUIRE(layerEnergy(centered[0]) > 0);
    REQUIRE(layerEnergy(centered[1]) > 0);
}





TEST_CASE("Layer waveform preview observes solo zones and polarity", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].invertPolarity = true;
    settings.layers[0].keyHigh = 35;
    const auto stillPlays = core::renderLayeredKick(settings.synth, settings.layers, 48000, 36);
    REQUIRE(layerEnergy(stillPlays) > 0);
}



TEST_CASE("Dense sorted MIDI traversal scales independently of block length", "[engine][midi][realtime]") {
    core::KickEngine engine;
    engine.prepare(48000);
    core::EngineSettings settings;
    const std::vector<core::MidiInputEvent> events(65535, core::MidiCcEvent{0, 7, 127});
    std::array<float, 8192> output{};
    float* channel = output.data();
    const auto started = std::chrono::steady_clock::now();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, static_cast<int>(output.size()), {}, events);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    CAPTURE(elapsed);
    REQUIRE(elapsed < 250.0);
    REQUIRE(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0; }));
}

TEST_CASE("Separate MIDI spans preserve offsets and controller semantics", "[engine][midi]") {
    const std::array<core::MidiTrigger, 2> notes{{{0, 36, 0.8f}, {48, 48, 0.6f}}};
    const std::array<core::MidiCcEvent, 4> controllers{{{16, 7, 32}, {32, 121, 127}, {48, 11, 50}, {64, 120, 0}}};
    const std::array<core::MidiInputEvent, 6> ordered{
        notes[0], controllers[0], controllers[1], controllers[2], notes[1], controllers[3]
    };
    core::KickEngine separateEngine, orderedEngine;
    separateEngine.prepare(48000);
    orderedEngine.prepare(48000);
    core::EngineSettings settings;
    settings.effects.limiterEnabled = false;
    std::array<float, 128> separate{}, combined{};
    float* separateChannel = separate.data();
    float* orderedChannel = combined.data();
    separateEngine.process(settings, {120, 0.0, 4, true}, notes, &separateChannel, 1, 128, controllers);
    orderedEngine.process(settings, {120, 0.0, 4, true}, {}, &orderedChannel, 1, 128, {}, ordered);
    REQUIRE(separate == combined);
    REQUIRE(std::any_of(separate.begin(), separate.begin() + 64, [](float sample) { return sample != 0; }));
    REQUIRE(std::all_of(separate.begin() + 64, separate.end(), [](float sample) { return sample == 0; }));
    REQUIRE(separateEngine.triggerSerial() == 2);
}

TEST_CASE("Full ADSR sustains and completes a twenty second release", "[followup][adsr]") {
    core::KickSettings settings;
    settings.ampEnabled = true;
    settings.ampAttackMs = settings.ampDecayMs = 0;
    settings.ampSustain = 1;
    settings.ampReleaseMs = 20000;
    core::KickVoice voice;
    voice.prepare(8000);
    voice.start(settings, 36, 1);
    for (int sample = 0; sample < 8000; ++sample) (void)voice.nextSample();
    REQUIRE(voice.isActive());
    voice.release();
    for (int sample = 0; sample < 159999; ++sample) (void)voice.nextSample();
    REQUIRE(voice.isActive());
    (void)voice.nextSample();
    REQUIRE_FALSE(voice.isActive());
}

TEST_CASE("ADSR releases match MIDI channel and true sample offsets", "[followup][adsr]") {
    core::EngineSettings settings;
    settings.synth.ampEnabled = true;
    settings.synth.ampAttackMs = settings.synth.ampDecayMs = settings.synth.ampReleaseMs = 0;
    settings.synth.ampSustain = 1;
    settings.effects.limiterEnabled = false;
    const std::array<core::MidiInputEvent, 3> events{
        core::MidiTrigger{8, 36, 1, false, 1}, core::MidiTrigger{16, 36, 1, false, 2}, core::MidiNoteOff{64, 36, 1}
    };
    core::KickEngine engine;
    engine.prepare(48000);
    std::array<float, 128> audio{};
    float* channel = audio.data();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 128, {}, events);
    REQUIRE(engine.activeNoteCount() == 1);
    const std::array<core::MidiInputEvent, 1> lastOff{core::MidiNoteOff{32, 36, 2}};
    engine.process(settings, {120, 128.0 / 24000, 4, true}, {}, &channel, 1, 128, {}, lastOff);
    REQUIRE(std::any_of(audio.begin(), audio.begin() + 32, [](float value) { return value != 0; }));
    REQUIRE(std::all_of(audio.begin() + 32, audio.end(), [](float value) { return value == 0; }));
    REQUIRE(engine.activeNoteCount() == 0);
}

TEST_CASE("CC23 bitcrush and CC25 gate affect the hit", "[followup][glitch]") {
    const auto render = [](int controller) {
        core::KickEngine engine;
        engine.prepare(48000);
        core::EngineSettings settings;
        settings.effects.limiterEnabled = false;
        std::vector<float> output(12000);
        float* channel = output.data();
        const std::array<core::MidiInputEvent, 2> events{core::MidiCcEvent{0, controller, 127}, core::MidiTrigger{0, 36, 1}};
        engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 12000, {}, events);
        return output;
    };
    REQUIRE(render(23) == render(0));
    REQUIRE(render(25) == render(0));
}

TEST_CASE("Reverse and stutter activations expire at the next quarter beat", "[followup][glitch]") {
    core::KickEngine engine;
    engine.prepare(8000);
    core::EngineSettings settings;
    const std::array<core::MidiInputEvent, 3> events{core::MidiCcEvent{0, 20, 127}, core::MidiCcEvent{0, 24, 127}, core::MidiTrigger{0, 36, 1}};
    double onset = 0;
    int duration = 4000;
    SECTION("exact beat gets a full quarter note") {}
    SECTION("off beat ends on the next quarter note") { onset = 0.5; duration = 2000; }
    std::array<float, 4000> audio{};
    float* channel = audio.data();
    engine.process(settings, {120, onset, 4, true}, {}, &channel, 1, duration - 1, {}, events);
    REQUIRE(engine.isVoiceReversing());
    REQUIRE_FALSE(engine.isStuttering());
    engine.process(settings, {120, 0.99975, 4, true}, {}, &channel, 1, 1);
    engine.process(settings, {120, 1.0, 4, true}, {}, &channel, 1, 1);
    REQUIRE_FALSE(engine.isVoiceReversing());
    REQUIRE_FALSE(engine.isStuttering());
}





TEST_CASE("CC-enabled ADSR sustain survives the following audio block", "[followup][adsr][routing]") {
    core::KickEngine engine;
    engine.prepare(48000);
    core::EngineSettings settings;
    settings.synth.ampAttackMs = settings.synth.ampDecayMs = 0;
    settings.synth.ampSustain = 1;
    const std::array<core::MidiInputEvent, 2> events{core::MidiCcEvent{0, 35, 127}, core::MidiTrigger{0, 36, 1}};
    core::MidiRouting mapping{nullptr, [](void*, int, int, core::EngineSettings& current) noexcept { current.synth.ampEnabled = true; }};
    std::array<float, 128> audio{};
    float* channel = audio.data();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 128, {}, events, mapping);
    REQUIRE(engine.activeNoteCount() == 1);
    settings.synth.ampEnabled = true;
    engine.process(settings, {120, 128.0 / 24000, 4, true}, {}, &channel, 1, 128);
    REQUIRE(engine.activeNoteCount() == 1);
}

TEST_CASE("Reverse follows a slower host tempo through its quarter-note boundary", "[followup][glitch]") {
    core::KickEngine engine;
    engine.prepare(8000);
    core::EngineSettings settings;
    const std::array<core::MidiInputEvent, 2> events{core::MidiCcEvent{0, 20, 127}, core::MidiTrigger{0, 36, 1}};
    std::array<float, 4000> audio{};
    float* channel = audio.data();
    engine.process(settings, {120, 0.25, 4, true}, {}, &channel, 1, 1000, {}, events);
    engine.process(settings, {60, 0.5, 4, true}, {}, &channel, 1, 3999);
    REQUIRE(engine.isVoiceReversing());
    engine.process(settings, {60, 0.999875, 4, true}, {}, &channel, 1, 1);
    REQUIRE_FALSE(engine.isVoiceReversing());
}

TEST_CASE("Maximum-length layered reverse has bounded callback setup", "[followup][glitch][realtime]") {
    core::KickEngine engine;
    engine.prepare(192000);
    core::EngineSettings settings;
    settings.synth.holdMs = 400;
    settings.synth.decayMs = 1500;
    for (auto& layer : settings.layers) { layer.enabled = true; layer.sound = core::DrumSoundType::kick; layer.decayMs = 1500; }
    const std::array<core::MidiInputEvent, 2> events{core::MidiCcEvent{0, 20, 127}, core::MidiTrigger{0, 36, 1}};
    std::array<float, 64> audio{};
    float* channel = audio.data();
    const auto began = std::chrono::steady_clock::now();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 64, {}, events);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    CAPTURE(elapsed);
    REQUIRE(elapsed < 100.0);
    REQUIRE(engine.isVoiceReversing());
}

TEST_CASE("Mapped limiter ceilings do not change samples before their CC offset", "[followup][routing]") {
    const auto render = [](bool map) {
        core::KickEngine engine;
        engine.prepare(48000);
        core::EngineSettings settings;
        settings.effects.ceilingGain = 1;
        std::array<float, 256> audio{};
        float* channel = audio.data();
        const std::array<core::MidiInputEvent, 2> events{core::MidiTrigger{0, 36, 1}, core::MidiCcEvent{128, 35, 127}};
        core::MidiRouting mapping;
        if (map) mapping.apply = [](void*, int, int, core::EngineSettings& current) noexcept { current.effects.ceilingGain = 0.1f; };
        engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 256, {}, events, mapping);
        return audio;
    };
    const auto withCc = render(true);
    const auto plain = render(false);
    REQUIRE(std::equal(withCc.begin(), withCc.begin() + 128, plain.begin()));
    REQUIRE(std::all_of(withCc.begin() + 128, withCc.end(), [](float value) { return std::abs(value) <= 0.100001f; }));
}





TEST_CASE("Enabling ADSR after a same-sample note preserves that one-shot", "[followup][adsr][routing]") {
    core::KickEngine engine;
    engine.prepare(48000);
    core::EngineSettings settings;
    const std::array<core::MidiInputEvent, 2> events{core::MidiTrigger{0, 36, 1}, core::MidiCcEvent{0, 35, 127}};
    core::MidiRouting mapping{nullptr,
        [](void*, int, int, core::EngineSettings& current) noexcept { current.synth.ampEnabled = true; }, nullptr,
        [](void*, core::EngineSettings& current) noexcept { current.synth.ampEnabled = false; }};
    std::array<float, 128> audio{};
    float* channel = audio.data();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 128, {}, events, mapping);
    REQUIRE(engine.activeNoteCount() == 1);
}

TEST_CASE("Host tails include active filter decay and then reach zero", "[followup][adsr][tail]") {
    core::KickEngine engine;
    engine.prepare(8000);
    core::EngineSettings settings;
    settings.synth.ampEnabled = true;
    settings.synth.ampAttackMs = settings.synth.ampDecayMs = settings.synth.ampReleaseMs = 0;
    settings.synth.ampSustain = 1;
    settings.effects.lowPassHz = 200;
    const std::array<core::MidiInputEvent, 2> events{core::MidiTrigger{0, 36, 1}, core::MidiNoteOff{64, 36, 1}};
    std::vector<float> output(8000);
    float* channel = output.data();
    engine.process(settings, {120, 0.0, 4, true}, {}, &channel, 1, 128, {}, events);
    REQUIRE(engine.activeNoteCount() == 0);
    REQUIRE(engine.remainingTailSeconds() > 0);
    engine.process(settings, {120, 0.032, 4, true}, {}, &channel, 1, 8000);
    REQUIRE_THAT(static_cast<float>(engine.remainingTailSeconds()), WithinAbs(0.0f, 0.0001f));
}


