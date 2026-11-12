#include "KickEngine.h"

#include <algorithm>
#include <cmath>

namespace openedmkick::core {
namespace {

[[nodiscard]] float unit(float value) noexcept {
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
}

[[nodiscard]] double safeTempo(double bpm) noexcept {
    return std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
}

} // namespace

void KickEngine::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    wavCache_.prepare(sampleRate_);
    forwardVoice_.prepare(sampleRate_);
    reverseVoice_.prepare(sampleRate_);
    stutter_.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);
    toneSmooth_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.012 * sampleRate_)));
    reset();
}

void KickEngine::reset() noexcept {
    fallbackPpq_ = 0.0;
    previousLockState_ = wavCache_.isLocked();
    currentPeak_ = currentBeatPhase_ = 0.0f;
    forwardVoice_.reset();
    reverseVoice_.reset();
    sequencer_.reset();
    crusher_.reset();
    stutter_.reset();
    filterPump_.reset();
    toneFilter_.reset();
    limiter_.reset();
    smoothedHighPassHz_ = ToneFilter::kHighPassBypassHz;
    smoothedLowPassHz_ = ToneFilter::kLowPassBypassHz;
    toneSnap_ = true;
    midiVolume_ = midiExpression_ = midiBrightness_ = 1.0f;
    reverseArmed_ = false;
    lastPpqStep_ = 0.0;
    hasLastPpq_ = false;
}

void KickEngine::lockCurrentKick(const KickSynthSettings& settings) {
    wavCache_.lockIn(settings, sampleRate_);
    previousLockState_ = true;
}

void KickEngine::unlockKick() noexcept {
    wavCache_.unlock();
    previousLockState_ = false;
}

bool KickEngine::restoreLockedWav(const std::vector<uint8_t>& wavBytes) {
    const bool restored = wavCache_.loadFromWavBytes(wavBytes);
    previousLockState_ = restored;
    return restored;
}

void KickEngine::triggerNote(
    const EngineSettings& settings,
    double samplePpq,
    double bpm,
    double beatsPerBar,
    float velocity,
    float semitoneOffset,
    float effectiveTailMs,
    bool reverseCue) noexcept {
    ++triggerSerial_;
    velocity = unit(velocity);
    const bool reverse = shouldTriggerReverse(settings.effects.reverseMode, reverseCue, samplePpq, beatsPerBar)
        && settings.effects.reverseMix > 0.001f;
    if (!reverse) {
        forwardVoice_.triggerForward(settings.synth, wavCache_, sampleRate_, velocity, semitoneOffset, effectiveTailMs);
        return;
    }

    const auto mode = settings.effects.reverseMode == ReverseSyncMode::Off
        ? ReverseSyncMode::BeatEnd : settings.effects.reverseMode;
    const double endPpq = reverseTargetPpq(mode, samplePpq, beatsPerBar);
    const int length = computeSyncedReverseSamples(mode, samplePpq, bpm, beatsPerBar, sampleRate_);
    if (length <= 0) {
        forwardVoice_.triggerForward(settings.synth, wavCache_, sampleRate_, velocity, semitoneOffset, effectiveTailMs);
        return;
    }

    reverseVoice_.triggerReverse(settings.synth, wavCache_, sampleRate_, length,
                                 velocity * unit(settings.effects.reverseMix), semitoneOffset);
    reverseVoice_.armMusicalReverse(samplePpq, endPpq);
    reverseArmed_ = true;
    reverseStartPpq_ = samplePpq;
    reverseEndPpq_ = endPpq;
    const float forwardLevel = 1.0f - unit(settings.effects.reverseMix);
    if (forwardLevel <= 0.001f) {
        forwardVoice_.reset();
    } else {
        forwardVoice_.triggerForward(settings.synth, wavCache_, sampleRate_, velocity * forwardLevel,
                                     semitoneOffset, effectiveTailMs);
    }
}

void KickEngine::process(
    const EngineSettings& settings,
    const Transport& transport,
    std::span<const MidiTrigger> midiTriggers,
    float* const* channels,
    int numChannels,
    int numSamples,
    std::span<const MidiCcEvent> controlChanges) noexcept {
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0) return;

    if (settings.lockIn && !previousLockState_ && !wavCache_.isLocked()) wavCache_.lockIn(settings.synth, sampleRate_);
    if (!settings.lockIn && previousLockState_) wavCache_.unlock();
    previousLockState_ = settings.lockIn;

    if (settings.masterBypass) {
        for (int channel = 0; channel < numChannels; ++channel) std::fill_n(channels[channel], numSamples, 0.0f);
        currentPeak_ = 0.0f;
        return;
    }

    const double bpm = safeTempo(transport.bpm);
    const double beatsPerBar = std::clamp(std::isfinite(transport.beatsPerBar) ? transport.beatsPerBar : 4.0, 1.0, 16.0);
    const double ppqPerSample = bpm / (60.0 * sampleRate_);
    const bool hasHostPpq = transport.ppqPosition.has_value() && std::isfinite(*transport.ppqPosition);
    const double blockPpq = hasHostPpq ? *transport.ppqPosition : fallbackPpq_;
    const bool autoActive = settings.autoEnabled && !settings.autoBypass;
    const int slot = std::clamp(settings.activeAutoSlot, 0, kNumAutoSlots - 1);
    const auto& autoSettings = settings.autoSlots[static_cast<size_t>(slot)];
    const auto division = [](int index) noexcept {
        return kSyncDivisionBeats[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kSyncDivisionBeats.size()) - 1))];
    };
    float peak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample) {
        const double ppq = blockPpq + static_cast<double>(sample) * ppqPerSample;
        currentBeatPhase_ = static_cast<float>(ppq - std::floor(ppq));

        const double expectedPpqStep = lastPpqStep_ > 0.0 ? lastPpqStep_ : ppqPerSample;
        const double seekTolerance = std::max(1.0e-7, expectedPpqStep * 0.5);
        const bool seekedBack = hasLastPpq_ && ppq + 1.0e-7 < lastSamplePpq_;
        const bool seekedForward = hasLastPpq_ && ppq > lastSamplePpq_ + expectedPpqStep + seekTolerance;
        if (reverseArmed_ && (!transport.isPlaying || seekedBack || seekedForward || ppq >= reverseEndPpq_)) {
            reverseVoice_.reset();
            reverseArmed_ = false;
        }
        hasLastPpq_ = true;
        lastSamplePpq_ = ppq;
        lastPpqStep_ = ppqPerSample;

        for (const auto& cc : controlChanges) {
            if (cc.sampleOffset != sample) continue;
            const float value = std::clamp(cc.value, 0, 127) / 127.0f;
            switch (cc.controller) {
                case 7: midiVolume_ = value; break;
                case 11: midiExpression_ = value; break;
                case 74: midiBrightness_ = value; break;
                case 120:
                    forwardVoice_.reset(); reverseVoice_.reset(); reverseArmed_ = false; break;
                case 121:
                    midiVolume_ = midiExpression_ = midiBrightness_ = 1.0f; break;
                default: break;
            }
        }

        if (autoActive && transport.isPlaying) {
            const auto event = sequencer_.stepSample(slot, autoSettings, settings.synth.tailLengthMs,
                bpm, beatsPerBar, sampleRate_, hasHostPpq, ppq);
            if (event.triggered)
                triggerNote(settings, ppq, bpm, beatsPerBar, event.velocity, event.semitoneOffset,
                            event.effectiveTailMs, event.reverseCue);
        }

        for (const auto& trigger : midiTriggers) {
            if (trigger.sampleOffset != sample || trigger.velocity <= 0.0f || (autoActive && !trigger.audition)) continue;
            const float transpose = settings.noteTracking && !trigger.audition
                ? trackedSemitoneOffset(settings.synth.note, settings.synth.octave, trigger.noteNumber) : 0.0f;
            triggerNote(settings, ppq, bpm, beatsPerBar, trigger.velocity, transpose, -1.0f, false);
        }

        const float raw = forwardVoice_.nextSample(settings.synth, wavCache_, sampleRate_, ppq)
            + reverseVoice_.nextSample(settings.synth, wavCache_, sampleRate_, ppq);
        const float driven = saturate(raw, settings.effects.drive);
        const float ramp = autoActive ? unit(autoSettings.crushSweep) * sequencer_.currentWindowPhase() : 0.0f;
        const float crushMix = std::clamp(unit(settings.effects.crushMix) + ramp, 0.0f, 1.0f);
        const float crushed = crusher_.process(driven, settings.effects.crushRateHz, settings.effects.crushBits, sampleRate_);
        const float postCrush = driven + crushMix * (crushed - driven);
        const float repeated = stutter_.process(postCrush, ppq, division(settings.effects.stutterDivision), bpm, sampleRate_);
        const float postRepeat = postCrush + unit(settings.effects.stutterMix) * (repeated - postCrush);
        const float pumped = filterPump_.process(postRepeat, ppq, division(settings.effects.pumpDivision),
                                                  settings.effects.pumpMix, sampleRate_);
        const float gated = pumped * TempoGate::gainAt(ppq, division(settings.effects.gateDivision), settings.effects.gateMix);
        float out = gated * settings.effects.outputGain * midiVolume_ * midiExpression_;

        if (toneSnap_) {
            smoothedHighPassHz_ = settings.effects.highPassHz;
            smoothedLowPassHz_ = settings.effects.lowPassHz;
            toneSnap_ = false;
        } else {
            smoothedHighPassHz_ += toneSmooth_ * (settings.effects.highPassHz - smoothedHighPassHz_);
            smoothedLowPassHz_ += toneSmooth_ * (settings.effects.lowPassHz - smoothedLowPassHz_);
        }
        const float brightnessMax = 40.0f + midiBrightness_ * (ToneFilter::kLowPassBypassHz - 40.0f);
        out = toneFilter_.process(out, smoothedHighPassHz_, std::min(smoothedLowPassHz_, brightnessMax), sampleRate_);
        if (!std::isfinite(out)) out = 0.0f;
        peak = std::max(peak, std::abs(out));
        for (int channel = 0; channel < numChannels; ++channel) channels[channel][sample] = out;
    }

    if (transport.isPlaying) fallbackPpq_ = blockPpq + static_cast<double>(numSamples) * ppqPerSample;
    if (settings.effects.limiterEnabled) limiter_.process(channels, numChannels, numSamples, settings.effects.ceilingGain);
    currentPeak_ = std::max(peak, currentPeak_ * 0.82f);
}

} // namespace openedmkick::core
