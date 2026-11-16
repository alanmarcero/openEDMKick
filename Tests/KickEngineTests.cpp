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

TEST_CASE("Regular mode triggers kick on any MIDI note and ignores Auto Mode Tail Shortener", "[engine]") {
    core::KickEngine engineA;
    core::KickEngine engineB;
    core::KickEngine engineHighNote;
    engineA.prepare(48000.0);
    engineB.prepare(48000.0);
    engineHighNote.prepare(48000.0);

    core::EngineSettings settingsShortTail{};
    settingsShortTail.autoEnabled = false;
    settingsShortTail.synth.decayMs = 450.0f;
    settingsShortTail.autoSlots[0].tailShorten = 0.0f;

    core::EngineSettings settingsMaxShorten = settingsShortTail;
    settingsMaxShorten.autoSlots[0].tailShorten = 1.0f;

    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> triggerLow{ { { 0, 24, 1.0f } } };
    const std::array<core::MidiTrigger, 1> triggerHigh{ { { 0, 96, 1.0f } } };

    std::vector<float> outA(12000, 0.0f);
    std::vector<float> outB(12000, 0.0f);
    float* ptrA = outA.data();
    float* ptrB = outB.data();

    engineA.process(settingsShortTail, transport, triggerLow, &ptrA, 1, 12000);
    engineB.process(settingsMaxShorten, transport, triggerLow, &ptrB, 1, 12000);

    const float tailEnergyA = std::accumulate(outA.begin() + 6000, outA.end(), 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });
    REQUIRE(tailEnergyA > 50.0f);

    std::ranges::for_each(std::views::iota(size_t{ 0 }, outA.size()), [&](size_t i) {
        REQUIRE_THAT(outA[i], WithinAbs(outB[i], 1e-6f));
    });

    std::vector<float> highNote(12000, 0.0f);
    float* highPointer = highNote.data();
    engineHighNote.process(settingsShortTail, transport, triggerHigh, &highPointer, 1, 12000);
    const double pitchDifference = std::inner_product(outA.begin(), outA.end(), highNote.begin(), 0.0,
        std::plus<>(), [](float a, float b) { return std::abs(static_cast<double>(a - b)); });
    REQUIRE(pitchDifference > 10.0);
}

TEST_CASE("Auto mode tail shortener reduces tail energy on rapid roll fills", "[engine]") {
    core::KickEngine unshortenedEngine;
    core::KickEngine shortenedEngine;
    unshortenedEngine.prepare(48000.0);
    shortenedEngine.prepare(48000.0);

    unshortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1); // Pulse Stamp
    shortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1);

    core::EngineSettings unshortened{};
    unshortened.autoEnabled = true;
    unshortened.synth.decayMs = 500.0f;
    unshortened.effects.limiterEnabled = false;
    unshortened.autoSlots[0].bars = 1;
    unshortened.autoSlots[0].fillPreset = 11;
    unshortened.autoSlots[0].tailShorten = 0.0f;

    core::EngineSettings shortened = unshortened;
    shortened.autoSlots[0].tailShorten = 0.95f;

    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    std::vector<float> bufLong(8000, 0.0f);
    std::vector<float> bufShort(8000, 0.0f);
    float* pLong = bufLong.data();
    float* pShort = bufShort.data();

    unshortenedEngine.process(unshortened, transport, {}, &pLong, 1, 8000);
    shortenedEngine.process(shortened, transport, {}, &pShort, 1, 8000);

    const float lateLong = std::accumulate(bufLong.begin() + 3000, bufLong.begin() + 5500, 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });
    const float lateShort = std::accumulate(bufShort.begin() + 3000, bufShort.begin() + 5500, 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });

    REQUIRE(lateShort < lateLong * 0.8f);
}

TEST_CASE("Auto mode stays silent while the host transport is stopped", "[engine]") {
    core::KickEngine engine;
    engine.prepare(48000.0);
    engine.sequencer().loadPresetIntoSlot(0, 11, 1);

    core::EngineSettings settings{};
    settings.autoEnabled = true;
    settings.synth.decayMs = 400.0f;
    settings.effects.limiterEnabled = false;
    settings.autoSlots[0].bars = 1;
    settings.autoSlots[0].fillPreset = 11;

    const core::Transport stopped{ 128.0, 0.0, 4.0, false };
    std::vector<float> buffer(4096, 0.0f);
    float* pointer = buffer.data();
    engine.process(settings, stopped, {}, &pointer, 1, static_cast<int>(buffer.size()));
    const float stoppedEnergy = std::accumulate(buffer.begin(), buffer.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    REQUIRE_THAT(stoppedEnergy, WithinAbs(0.0f, 1.0e-6f));

    engine.reset();
    const core::Transport playing{ 128.0, 0.0, 4.0, true };
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    engine.process(settings, playing, {}, &pointer, 1, static_cast<int>(buffer.size()));
    const float playingEnergy = std::accumulate(buffer.begin(), buffer.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    REQUIRE(playingEnergy > 20.0f);
}

TEST_CASE("MIDI note-on and audition both trigger during Auto Mode", "[engine]") {
    core::KickEngine midiTriggered;
    core::KickEngine auditioned;
    midiTriggered.prepare(48000.0);
    auditioned.prepare(48000.0);
    midiTriggered.sequencer().setNotesForSlot(0, {});
    auditioned.sequencer().setNotesForSlot(0, {});

    core::EngineSettings settings{};
    settings.autoEnabled = true;
    settings.effects.limiterEnabled = false;
    settings.autoSlots[0].bars = 1;
    const core::Transport transport{ 120.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> midi{ { { 0, 60, 1.0f, false } } };
    const std::array<core::MidiTrigger, 1> audition{ { { 0, 60, 1.0f, true } } };

    std::vector<float> midiOutput(2048, 0.0f);
    std::vector<float> heard(2048, 0.0f);
    float* midiPtr = midiOutput.data();
    float* heardPtr = heard.data();
    midiTriggered.process(settings, transport, midi, &midiPtr, 1, static_cast<int>(midiOutput.size()));
    auditioned.process(settings, transport, audition, &heardPtr, 1, static_cast<int>(heard.size()));

    const float midiEnergy = std::accumulate(midiOutput.begin(), midiOutput.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    const float heardEnergy = std::accumulate(heard.begin(), heard.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    REQUIRE(midiEnergy > 10.0f);
    REQUIRE(heardEnergy > 10.0f);
}

TEST_CASE("Hot kicks stay finite and under the limiter ceiling", "[engine]") {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.synth.drive = 1.0f;
    settings.synth.clickLevel = 1.0f;
    settings.synth.sweepOctaves = 5.0f;
    settings.effects.drive = 1.0f;
    settings.effects.outputGain = 2.0f;
    settings.effects.limiterEnabled = true;
    settings.effects.ceilingGain = 0.5f;
    settings.effects.crushMix = 1.0f;
    settings.effects.crushBits = 3;
    settings.effects.crushRateHz = 1500.0f;
    settings.effects.pumpMix = 1.0f;
    settings.effects.stutterMix = 0.5f;
    const core::Transport transport{ 140.0, 1.25, 4.0, true };
    const std::array<core::MidiTrigger, 1> trigger{ { { 0, 36, 1.0f, false } } };
    std::vector<float> buffer(4096, 0.0f);
    float* pointer = buffer.data();
    engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(buffer.size()));
    std::ranges::for_each(buffer, [](float sample) {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 0.5001f);
    });
}

namespace {

[[nodiscard]] std::vector<float> renderFilteredKick(float highPassHz, float lowPassHz) {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.effects.drive = 0.0f;
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
        settings.synth.drive = 0.0f;
        settings.synth.clip = 0.0f;
        settings.synth.holdMs = 0.0f;
        settings.synth.decayMs = 360.0f;
        settings.effects.drive = 0.0f;
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
        settings.synth.drive = 0.0f;
        settings.synth.clip = 0.0f;
        settings.synth.holdMs = 0.0f;
        settings.synth.decayMs = 120.0f;
        settings.effects.limiterEnabled = false;
        settings.effects.drive = 0.0f;
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
        settings.effects.drive = 0.0f;
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

TEST_CASE("Same-sample controllers are applied before Auto Mode triggers", "[engine][midi][auto]") {
    const auto renderAutoHit = [](int controller) {
        core::KickEngine engine;
        engine.prepare(48000.0);
        engine.sequencer().setNotesForSlot(0, { { 0, 4, 1.0f, false } });
        core::EngineSettings settings{};
        settings.autoEnabled = true;
        settings.autoSlots[0].bars = 1;
        const std::array<core::MidiInputEvent, 1> events{
            core::MidiInputEvent{ std::in_place_type<core::MidiCcEvent>, core::MidiCcEvent{ 0, controller, 127 } }
        };
        std::vector<float> output(128, 0.0f);
        float* channel = output.data();
        engine.process(settings, core::Transport{ 120.0, 0.0, 4.0, true }, {},
                       &channel, 1, static_cast<int>(output.size()), {}, events);
        const double energy = std::accumulate(output.begin(), output.end(), 0.0,
            [](double sum, float sample) { return sum + std::abs(static_cast<double>(sample)); });
        return std::pair{ engine.isVoiceReversing(), energy };
    };

    REQUIRE(renderAutoHit(20).first);
    REQUIRE(renderAutoHit(120).second > 0.0);
}

namespace {
std::array<std::vector<float>, 2> renderImprovedLayers(core::EngineSettings settings, int note = 36, float velocity = 0.5f) {
    core::KickEngine engine;
    engine.prepare(48000);
    settings.effects.drive = 0;
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
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 35, 64.0f / 127)[0]) == 0);
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 36, 63.0f / 127)[0]) == 0);
    REQUIRE(layerEnergy(renderImprovedLayers(settings, 36, 97.0f / 127)[0]) == 0);
}

TEST_CASE("Layer synthesis offsets leave other layers independent", "[layer-improvements]") {
    core::EngineSettings settings;
    const auto original = renderImprovedLayers(settings);
    settings.layers[1].driveOffset = 0.4f;
    settings.layers[1].toneScale = 0.2f;
    settings.layers[1].sweepScale = 0;
    REQUIRE(renderImprovedLayers(settings) == original);
    settings.layers[0].driveOffset = 0.4f;
    settings.layers[0].toneScale = 0.2f;
    settings.layers[0].sweepScale = 0;
    REQUIRE(renderImprovedLayers(settings) != original);
}

TEST_CASE("Layer solo pan and polarity act before the stereo effects", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].pan = -1;
    const auto left = renderImprovedLayers(settings);
    REQUIRE(layerEnergy(left[0]) > 0);
    REQUIRE(layerEnergy(left[1]) == 0);
    settings.layers[0].invertPolarity = true;
    const auto inverted = renderImprovedLayers(settings);
    REQUIRE_THAT(inverted[0][300], WithinAbs(-left[0][300], 1e-6));
    settings.layers[1].enabled = true;
    settings.layers[1].solo = true;
    const auto solo = renderImprovedLayers(settings);
    settings.layers[0].enabled = false;
    REQUIRE(renderImprovedLayers(settings) == solo);
}

TEST_CASE("Snare drive offset changes its own synthesized signal", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].sound = core::DrumSoundType::snare;
    const double original = layerEnergy(renderImprovedLayers(settings)[0]);
    settings.layers[0].driveOffset = 0.7f;
    REQUIRE(layerEnergy(renderImprovedLayers(settings)[0]) != original);
}

TEST_CASE("Layer sequencer routing only starts the addressed voice", "[layer-improvements]") {
    core::KickEngine engine;
    engine.prepare(48000);
    engine.sequencer().setNotesForSlot(0, {{0, 4, 1.0f, false, 1, 1}});
    core::EngineSettings settings;
    settings.autoEnabled = true;
    settings.layers[1].enabled = true;
    settings.layers[1].pan = 1;
    settings.effects.limiterEnabled = false;
    settings.effects.drive = 0;
    std::array<float, 4096> left{}, right{};
    float* channels[]{left.data(), right.data()};
    engine.process(settings, {120, 0.0, 4, true}, {}, channels, 2, 4096);
    REQUIRE(std::all_of(left.begin(), left.end(), [](float sample) { return sample == 0; }));
    REQUIRE(std::any_of(right.begin(), right.end(), [](float sample) { return sample != 0; }));
    REQUIRE(engine.triggerSerial() == 1);
}

TEST_CASE("Layer waveform preview observes solo zones and polarity", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[1] = settings.layers[0];
    settings.layers[1].invertPolarity = true;
    const auto cancelled = core::renderLayeredKick(settings.synth, settings.layers, 48000, 36);
    REQUIRE(layerEnergy(cancelled) == 0);
    settings.layers[0].solo = true;
    const auto solo = core::renderLayeredKick(settings.synth, settings.layers, 48000, 36);
    REQUIRE(layerEnergy(solo) > 0);
    settings.layers[0].keyHigh = 35;
    const auto outsideZone = core::renderLayeredKick(settings.synth, settings.layers, 48000, 36);
    REQUIRE(layerEnergy(outsideZone) == 0);
}

TEST_CASE("Hat tone scale changes its filtered metal and noise layer", "[layer-improvements]") {
    core::EngineSettings settings;
    settings.layers[0].sound = core::DrumSoundType::hat;
    const auto original = renderImprovedLayers(settings);
    settings.layers[0].toneScale = 0.3f;
    const auto dark = renderImprovedLayers(settings);
    REQUIRE(layerEnergy(dark[0]) != layerEnergy(original[0]));
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
