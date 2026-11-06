#include "KickEngine.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numeric>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("Regular mode triggers kick on any MIDI note and ignores Auto Mode Tail Shortener", "[engine]") {
    core::KickEngine engineA;
    core::KickEngine engineB;
    engineA.prepare(48000.0);
    engineB.prepare(48000.0);

    core::EngineSettings settingsShortTail{};
    settingsShortTail.autoEnabled = false;
    settingsShortTail.synth.tailLengthMs = 450.0f;
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
    engineB.process(settingsMaxShorten, transport, triggerHigh, &ptrB, 1, 12000);

    const float tailEnergyA = std::accumulate(outA.begin() + 6000, outA.end(), 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });
    REQUIRE(tailEnergyA > 50.0f);

    std::ranges::for_each(std::views::iota(size_t{ 0 }, outA.size()), [&](size_t i) {
        REQUIRE_THAT(outA[i], WithinAbs(outB[i], 1e-6f));
    });
}

TEST_CASE("Auto mode tail shortener reduces tail energy on rapid 1/64 rolls", "[engine]") {
    core::KickEngine unshortenedEngine;
    core::KickEngine shortenedEngine;
    unshortenedEngine.prepare(48000.0);
    shortenedEngine.prepare(48000.0);

    unshortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1); // Mainstage 64th Machine Gun
    shortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1);

    core::EngineSettings unshortened{};
    unshortened.autoEnabled = true;
    unshortened.synth.tailLengthMs = 500.0f;
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

    REQUIRE(lateShort < lateLong * 0.55f);
}

TEST_CASE("Auto mode stays silent while the host transport is stopped", "[engine]") {
    core::KickEngine engine;
    engine.prepare(48000.0);
    engine.sequencer().loadPresetIntoSlot(0, 11, 1);

    core::EngineSettings settings{};
    settings.autoEnabled = true;
    settings.synth.tailLengthMs = 400.0f;
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

TEST_CASE("Audition triggers during Auto Mode while ordinary MIDI does not", "[engine]") {
    core::KickEngine ignored;
    core::KickEngine auditioned;
    ignored.prepare(48000.0);
    auditioned.prepare(48000.0);
    ignored.sequencer().setNotesForSlot(0, {});
    auditioned.sequencer().setNotesForSlot(0, {});

    core::EngineSettings settings{};
    settings.autoEnabled = true;
    settings.effects.limiterEnabled = false;
    settings.autoSlots[0].bars = 1;
    const core::Transport transport{ 120.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> midi{ { { 0, 60, 1.0f, false } } };
    const std::array<core::MidiTrigger, 1> audition{ { { 0, 60, 1.0f, true } } };

    std::vector<float> silent(2048, 0.0f);
    std::vector<float> heard(2048, 0.0f);
    float* silentPtr = silent.data();
    float* heardPtr = heard.data();
    ignored.process(settings, transport, midi, &silentPtr, 1, static_cast<int>(silent.size()));
    auditioned.process(settings, transport, audition, &heardPtr, 1, static_cast<int>(heard.size()));

    const float silentEnergy = std::accumulate(silent.begin(), silent.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    const float heardEnergy = std::accumulate(heard.begin(), heard.end(), 0.0f,
        [](float acc, float sample) { return acc + std::abs(sample); });
    REQUIRE_THAT(silentEnergy, WithinAbs(0.0f, 1.0e-6f));
    REQUIRE(heardEnergy > 10.0f);
}

TEST_CASE("Hot kicks stay finite and under the limiter ceiling", "[engine]") {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.synth.subDrive = 1.0f;
    settings.synth.clickLevel = 1.0f;
    settings.synth.punch = 1.0f;
    settings.synth.startPitchHz = 1800.0f;
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
    settings.synth.tailLengthMs = 380.0f;
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

void writeWav(const std::string& path, const std::vector<float>& samples) {
    const auto bytes = core::encodeWavBytes(samples, 48000u);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
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

    if (const char* directory = std::getenv("OEK_RENDER_DIR")) {
        const int gap = static_cast<int>(0.08 * 48000.0);
        std::vector<float> comparison;
        const std::array<const std::vector<float>*, 3> order{ &dry, &highPassed, &lowPassed };
        std::ranges::for_each(order, [&](const std::vector<float>* hit) {
            comparison.insert(comparison.end(), hit->begin(), hit->end());
            comparison.insert(comparison.end(), static_cast<size_t>(gap), 0.0f);
        });
        const std::string root(directory);
        writeWav(root + "/filter-dry.wav", dry);
        writeWav(root + "/filter-highpass.wav", highPassed);
        writeWav(root + "/filter-lowpass.wav", lowPassed);
        writeWav(root + "/filter-comparison.wav", comparison);
    }
}

namespace {

[[nodiscard]] int risingCrossings(const std::vector<float>& samples, int start, int end) {
    int count = 0;
    const int i0 = std::clamp(start, 1, static_cast<int>(samples.size()));
    const int i1 = std::clamp(end, i0, static_cast<int>(samples.size()));
    std::ranges::for_each(std::views::iota(i0, i1), [&](int i) {
        const float previous = samples[static_cast<size_t>(i - 1)];
        const float current = samples[static_cast<size_t>(i)];
        if (previous <= 0.0f && current > 0.0f) {
            ++count;
        }
    });
    return count;
}

[[nodiscard]] std::vector<float> playMidiKick(bool noteTracking, int note, float velocity, bool lockWav = false) {
    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.noteTracking = noteTracking;
    settings.effects.drive = 0.0f;
    settings.effects.limiterEnabled = false;
    settings.synth.tailLengthMs = 360.0f;
    settings.synth.pitchDecayMs = 30.0f;
    settings.synth.note = 5;
    settings.synth.octave = 1;
    if (lockWav) {
        engine.lockCurrentKick(settings.synth);
        settings.lockIn = true;
    }
    const core::Transport transport{ 120.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> trigger{ { { 0, note, velocity, false } } };
    std::vector<float> buffer(18000, 0.0f);
    float* pointer = buffer.data();
    engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(buffer.size()));
    return buffer;
}

[[nodiscard]] double totalEnergy(const std::vector<float>& samples) {
    return std::accumulate(samples.begin(), samples.end(), 0.0, [](double acc, float sample) {
        return acc + std::abs(static_cast<double>(sample));
    });
}

} // namespace

TEST_CASE("Fixed ignores note number and Note tracking follows the MIDI root", "[engine][midi]") {
    REQUIRE(core::midiRootForTuning(5, 1) == 29);
    REQUIRE(core::midiRootForTuning(0, 1) == 24);
    REQUIRE(core::midiRootForTuning(0, 2) == 36);
    REQUIRE_THAT(core::trackedSemitoneOffset(5, 1, 41), WithinAbs(12.0f, 0.001f));

    const auto fixedLow = playMidiKick(false, 29, 1.0f);
    const auto fixedHigh = playMidiKick(false, 41, 1.0f);
    std::ranges::for_each(std::views::iota(size_t{ 0 }, fixedLow.size()), [&](size_t i) {
        REQUIRE_THAT(fixedHigh[i], WithinAbs(fixedLow[i], 1.0e-5f));
    });

    const auto trackedRoot = playMidiKick(true, 29, 1.0f);
    const auto trackedOctave = playMidiKick(true, 41, 1.0f);
    const int tailStart = 9000;
    const int tailEnd = 16000;
    const int rootCrossings = risingCrossings(trackedRoot, tailStart, tailEnd);
    const int octaveCrossings = risingCrossings(trackedOctave, tailStart, tailEnd);
    REQUIRE(rootCrossings > 4);
    REQUIRE(octaveCrossings > rootCrossings * 3 / 2);

    const auto lockedRoot = playMidiKick(true, 29, 1.0f, true);
    const auto lockedOctave = playMidiKick(true, 41, 1.0f, true);
    REQUIRE(risingCrossings(lockedOctave, 2000, 7000) > risingCrossings(lockedRoot, 2000, 7000));
    const auto lastLoud = [](const std::vector<float>& samples) {
        for (int i = static_cast<int>(samples.size()) - 1; i >= 0; --i) {
            if (std::abs(samples[static_cast<size_t>(i)]) > 0.01f) {
                return i;
            }
        }
        return -1;
    };
    REQUIRE(lastLoud(lockedOctave) < lastLoud(lockedRoot) * 7 / 10);

    const auto full = playMidiKick(false, 60, 1.0f);
    const auto half = playMidiKick(false, 60, 0.5f);
    const auto quiet = playMidiKick(false, 60, 0.25f);
    const auto silent = playMidiKick(false, 60, 0.0f);
    const auto lockedFull = playMidiKick(false, 60, 1.0f, true);
    const auto lockedHalf = playMidiKick(false, 60, 0.5f, true);
    REQUIRE(totalEnergy(silent) < 1.0e-4);
    REQUIRE(totalEnergy(full) > totalEnergy(half) * 1.7);
    REQUIRE(totalEnergy(full) < totalEnergy(half) * 2.4);
    REQUIRE(totalEnergy(half) > totalEnergy(quiet) * 1.7);
    REQUIRE(totalEnergy(lockedFull) > totalEnergy(lockedHalf) * 1.7);
    REQUIRE(totalEnergy(lockedFull) < totalEnergy(lockedHalf) * 2.4);

    core::KickEngine layered;
    layered.prepare(48000.0);
    core::EngineSettings layerSettings{};
    layerSettings.effects.drive = 0.0f;
    layerSettings.effects.limiterEnabled = false;
    layerSettings.synth.snareLevel = 1.0f;
    layerSettings.synth.clapLevel = 0.8f;
    layerSettings.synth.hatLevel = 0.7f;
    const core::Transport layerTransport{ 120.0, 0.0, 4.0, true };
    auto layeredEnergy = [&](float velocity) {
        layered.reset();
        const std::array<core::MidiTrigger, 1> trig{ { { 0, 36, velocity, false } } };
        std::vector<float> buffer(8000, 0.0f);
        float* pointer = buffer.data();
        layered.process(layerSettings, layerTransport, trig, &pointer, 1, 8000);
        return totalEnergy(buffer);
    };
    const double layeredFull = layeredEnergy(1.0f);
    const double layeredHalf = layeredEnergy(0.5f);
    REQUIRE(layeredFull > layeredHalf * 1.7);
    REQUIRE(layeredFull < layeredHalf * 2.4);

    core::KickEngine engine;
    engine.prepare(48000.0);
    core::EngineSettings settings{};
    settings.effects.drive = 0.0f;
    settings.effects.limiterEnabled = false;
    const core::Transport transport{ 120.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 2> retrigger{ { { 0, 36, 1.0f, false }, { 6000, 48, 1.0f, false } } };
    std::vector<float> both(12000, 0.0f);
    float* bothPtr = both.data();
    engine.process(settings, transport, retrigger, &bothPtr, 1, 12000);
    const double firstHit = std::accumulate(both.begin() + 100, both.begin() + 2000, 0.0, [](double acc, float sample) {
        return acc + std::abs(sample);
    });
    const double secondHit = std::accumulate(both.begin() + 6100, both.begin() + 8000, 0.0, [](double acc, float sample) {
        return acc + std::abs(sample);
    });
    REQUIRE(firstHit > 5.0);
    REQUIRE(secondHit > 5.0);

    const std::array<core::MidiCcEvent, 1> soundOff{ { { 400, 120, 0 } } };
    engine.reset();
    std::vector<float> choked(4000, 0.0f);
    float* chokedPtr = choked.data();
    const std::array<core::MidiTrigger, 1> one{ { { 0, 40, 1.0f, false } } };
    engine.process(settings, transport, one, &chokedPtr, 1, 4000, soundOff);
    const double afterOff = std::accumulate(choked.begin() + 800, choked.end(), 0.0, [](double acc, float sample) {
        return acc + std::abs(sample);
    });
    REQUIRE(afterOff < 1.0e-3);
    REQUIRE(std::accumulate(choked.begin() + 50, choked.begin() + 300, 0.0, [](double acc, float sample) {
        return acc + std::abs(sample);
    }) > 1.0);

    engine.reset();
    const std::array<core::MidiCcEvent, 1> volumeOff{ { { 0, 7, 0 } } };
    std::vector<float> muted(2000, 0.0f);
    float* mutedPtr = muted.data();
    engine.process(settings, transport, one, &mutedPtr, 1, 2000, volumeOff);
    REQUIRE(totalEnergy(muted) < 1.0e-4);

    const std::array<core::MidiCcEvent, 1> resetCc{ { { 0, 121, 0 } } };
    std::vector<float> restored(2000, 0.0f);
    float* restoredPtr = restored.data();
    engine.process(settings, transport, one, &restoredPtr, 1, 2000, resetCc);
    REQUIRE(totalEnergy(restored) > 10.0);

    if (const char* directory = std::getenv("OEK_RENDER_DIR")) {
        const int gap = static_cast<int>(0.08 * 48000.0);
        const std::array<std::vector<float>, 7> hits{
            fixedLow, fixedHigh, trackedRoot, trackedOctave, full, half, quiet
        };
        std::vector<float> comparison;
        std::ranges::for_each(hits, [&](const std::vector<float>& hit) {
            comparison.insert(comparison.end(), hit.begin(), hit.end());
            comparison.insert(comparison.end(), static_cast<size_t>(gap), 0.0f);
        });
        const std::string root(directory);
        writeWav(root + "/midi-fixed-root.wav", fixedLow);
        writeWav(root + "/midi-fixed-plus12.wav", fixedHigh);
        writeWav(root + "/midi-track-root.wav", trackedRoot);
        writeWav(root + "/midi-track-plus12.wav", trackedOctave);
        writeWav(root + "/midi-velocity-full.wav", full);
        writeWav(root + "/midi-velocity-half.wav", half);
        writeWav(root + "/midi-velocity-quarter.wav", quiet);
        writeWav(root + "/midi-comparison.wav", comparison);
    }
}
