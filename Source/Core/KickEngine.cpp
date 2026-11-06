#include "KickEngine.h"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace openedmkick::core {

void KickEngine::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    wavCache_.prepare(sampleRate_);
    stutter_.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);
    toneSmooth_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.015 * sampleRate_)));
    forwardVoice_.prepare(sampleRate_);
    reverseVoice_.prepare(sampleRate_);
    reset();
}

void KickEngine::reset() noexcept {
    fallbackPpq_ = 0.0;
    currentPeak_ = 0.0f;
    currentBeatPhase_ = 0.0f;
    forwardVoice_.reset();
    reverseVoice_.reset();
    sequencer_.reset();
    crusher_.reset();
    stutter_.reset();
    filterPump_.reset();
    toneFilter_.reset();
    smoothedHighPassHz_ = ToneFilter::kHighPassBypassHz;
    smoothedLowPassHz_ = ToneFilter::kLowPassBypassHz;
    toneSnap_ = true;
    midiVolume_ = 1.0f;
    midiExpression_ = 1.0f;
    midiBrightness_ = 1.0f;
    reverseArmed_ = false;
    hasLastPpq_ = false;
    limiter_.reset();
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
    const bool ok = wavCache_.loadFromWavBytes(wavBytes);
    if (ok) {
        previousLockState_ = true;
    }
    return ok;
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
    const bool useReverse = shouldTriggerReverse(
        settings.effects.reverseMode,
        reverseCue,
        samplePpq,
        beatsPerBar);

    if (useReverse && settings.effects.reverseMix > 0.001f) {
        const auto effectiveMode = settings.effects.reverseMode == ReverseSyncMode::Off
            ? ReverseSyncMode::BeatEnd
            : settings.effects.reverseMode;
        const int reverseSamples = computeSyncedReverseSamples(
            effectiveMode,
            samplePpq,
            bpm,
            beatsPerBar,
            sampleRate_);
        if (reverseSamples <= 0) {
            forwardVoice_.triggerForward(
                settings.synth,
                wavCache_,
                sampleRate_,
                velocity,
                semitoneOffset,
                effectiveTailMs);
            return;
        }
        reverseVoice_.triggerReverse(
            settings.synth,
            wavCache_,
            sampleRate_,
            reverseSamples,
            velocity * settings.effects.reverseMix,
            semitoneOffset);
        const double reverseEnd = reverseTargetPpq(effectiveMode, samplePpq, beatsPerBar);
        reverseVoice_.armMusicalReverse(samplePpq, reverseEnd);
        reverseArmed_ = true;
        reverseStartPpq_ = samplePpq;
        reverseEndPpq_ = reverseEnd;
        if (settings.effects.reverseMix >= 0.999f) {
            forwardVoice_.reset();
            return;
        }
    }

    const float forwardGain = useReverse ? (1.0f - settings.effects.reverseMix) : 1.0f;
    forwardVoice_.triggerForward(
        settings.synth,
        wavCache_,
        sampleRate_,
        velocity * forwardGain,
        semitoneOffset,
        effectiveTailMs);
}

void KickEngine::process(
    const EngineSettings& settings,
    const Transport& transport,
    std::span<const MidiTrigger> midiTriggers,
    float* const* channels,
    int numChannels,
    int numSamples,
    std::span<const MidiCcEvent> controlChanges) noexcept {
    if (numChannels <= 0 || numSamples <= 0) {
        return;
    }

    if (settings.lockIn && !previousLockState_ && !wavCache_.isLocked()) {
        wavCache_.lockIn(settings.synth, sampleRate_);
    }
    if (!settings.lockIn && previousLockState_) {
        wavCache_.unlock();
    }
    previousLockState_ = settings.lockIn;

    if (settings.masterBypass) {
        std::ranges::for_each(std::views::iota(0, numChannels), [&](int ch) {
            std::fill_n(channels[ch], numSamples, 0.0f);
        });
        currentPeak_ = 0.0f;
        return;
    }

    const double safeBpm = std::max(20.0, transport.bpm);
    const double safeBeatsPerBar = std::max(1.0, transport.beatsPerBar);
    const double ppqPerSample = safeBpm / (60.0 * sampleRate_);
    const bool hasHostPpq = transport.ppqPosition.has_value();
    const double blockStartPpq = hasHostPpq ? *transport.ppqPosition : fallbackPpq_;
    const bool autoActive = settings.autoEnabled && !settings.autoBypass;
    const int activeSlot = std::clamp(settings.activeAutoSlot, 0, kNumAutoSlots - 1);
    const auto& slotSettings = settings.autoSlots[static_cast<size_t>(activeSlot)];

    const auto safeDiv = [](int index) {
        const size_t clamped = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kSyncDivisionBeats.size()) - 1));
        return kSyncDivisionBeats[clamped];
    };
    const double stutterBeats = safeDiv(settings.effects.stutterDivision);
    const double gateBeats = safeDiv(settings.effects.gateDivision);
    const double pumpBeats = safeDiv(settings.effects.pumpDivision);

    float blockPeak = 0.0f;

    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sampleIdx) {
        const double samplePpq = blockStartPpq + static_cast<double>(sampleIdx) * ppqPerSample;
        currentBeatPhase_ = static_cast<float>(samplePpq - std::floor(samplePpq));

        if (reverseArmed_ && (!transport.isPlaying
                || (hasLastPpq_ && samplePpq + 1.0e-7 < lastSamplePpq_)
                || samplePpq >= reverseEndPpq_)) {
            reverseVoice_.reset();
            reverseArmed_ = false;
        }
        hasLastPpq_ = true;
        lastSamplePpq_ = samplePpq;

        std::ranges::for_each(controlChanges, [&](const MidiCcEvent& event) {
            if (event.sampleOffset != sampleIdx) {
                return;
            }
            const float unit = std::clamp(event.value, 0, 127) / 127.0f;
            switch (event.controller) {
                case 7:
                    midiVolume_ = unit;
                    break;
                case 11:
                    midiExpression_ = unit;
                    break;
                case 74:
                    midiBrightness_ = unit;
                    break;
                case 120:
                    forwardVoice_.reset();
                    reverseVoice_.reset();
                    reverseArmed_ = false;
                    break;
                case 121:
                    midiVolume_ = 1.0f;
                    midiExpression_ = 1.0f;
                    midiBrightness_ = 1.0f;
                    break;
                case 123:
                    break;
                default:
                    break;
            }
        });

        if (autoActive && transport.isPlaying) {
            const auto autoEvent = sequencer_.stepSample(
                activeSlot,
                slotSettings,
                settings.synth.tailLengthMs,
                safeBpm,
                safeBeatsPerBar,
                sampleRate_,
                hasHostPpq,
                samplePpq);
            if (autoEvent.triggered) {
                triggerNote(
                    settings,
                    samplePpq,
                    safeBpm,
                    safeBeatsPerBar,
                    autoEvent.velocity,
                    autoEvent.semitoneOffset,
                    autoEvent.effectiveTailMs,
                    autoEvent.reverseCue);
            }
        }

        std::ranges::for_each(midiTriggers, [&](const MidiTrigger& trig) {
            // Regular Mode plays the tuned kick for every note. Auto Mode ignores
            // MIDI except explicit auditions, and the tail shortener stays off.
            if ((!autoActive || trig.audition) && trig.sampleOffset == sampleIdx && trig.velocity > 0.0f) {
                const float semitones = (settings.noteTracking && !trig.audition)
                    ? trackedSemitoneOffset(settings.synth.note, settings.synth.octave, trig.noteNumber)
                    : 0.0f;
                triggerNote(
                    settings,
                    samplePpq,
                    safeBpm,
                    safeBeatsPerBar,
                    trig.velocity,
                    semitones,
                    -1.0f,
                    false);
            }
        });

        const float rawKick = forwardVoice_.nextSample(settings.synth, wavCache_, sampleRate_, samplePpq)
            + reverseVoice_.nextSample(settings.synth, wavCache_, sampleRate_, samplePpq);

        const float driven = saturate(rawKick, settings.effects.drive);

        const float autoCrushBoost = autoActive
            ? (slotSettings.crushSweep * sequencer_.currentWindowPhase())
            : 0.0f;
        const float effectiveCrushMix = std::clamp(settings.effects.crushMix + autoCrushBoost, 0.0f, 1.0f);
        const float crushed = crusher_.process(
            driven,
            settings.effects.crushRateHz,
            settings.effects.crushBits,
            sampleRate_);
        const float postCrush = driven + effectiveCrushMix * (crushed - driven);

        const float stuttered = stutter_.process(
            postCrush,
            samplePpq,
            stutterBeats,
            safeBpm,
            sampleRate_);
        const float postStutter = postCrush + std::clamp(settings.effects.stutterMix, 0.0f, 1.0f) * (stuttered - postCrush);

        const float pumped = filterPump_.process(
            postStutter,
            samplePpq,
            pumpBeats,
            settings.effects.pumpMix,
            sampleRate_);

        const float gateGain = TempoGate::gainAt(samplePpq, gateBeats, settings.effects.gateMix);
        float finalSample = pumped * gateGain * settings.effects.outputGain;
        if (toneSnap_) {
            smoothedHighPassHz_ = settings.effects.highPassHz;
            smoothedLowPassHz_ = settings.effects.lowPassHz;
            toneSnap_ = false;
        } else {
            smoothedHighPassHz_ += toneSmooth_ * (settings.effects.highPassHz - smoothedHighPassHz_);
            smoothedLowPassHz_ += toneSmooth_ * (settings.effects.lowPassHz - smoothedLowPassHz_);
        }
        const float brightnessCutoff = 40.0f + midiBrightness_ * (ToneFilter::kLowPassBypassHz - 40.0f);
        const float lowPassHz = std::min(smoothedLowPassHz_, brightnessCutoff);
        finalSample *= midiVolume_ * midiExpression_;
        finalSample = toneFilter_.process(finalSample, smoothedHighPassHz_, lowPassHz, sampleRate_);
        if (!std::isfinite(finalSample)) {
            finalSample = 0.0f;
        }

        blockPeak = std::max(blockPeak, std::abs(finalSample));
        std::ranges::for_each(std::views::iota(0, numChannels), [&](int ch) {
            channels[ch][sampleIdx] = finalSample;
        });
    });

    if (transport.isPlaying) {
        fallbackPpq_ = blockStartPpq + static_cast<double>(numSamples) * ppqPerSample;
    }

    if (settings.effects.limiterEnabled) {
        limiter_.process(channels, numChannels, numSamples, settings.effects.ceilingGain);
    }

    currentPeak_ = std::max(blockPeak, currentPeak_ * 0.85f);
}

} // namespace openedmkick::core
