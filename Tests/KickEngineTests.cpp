#include "KickEngine.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
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
