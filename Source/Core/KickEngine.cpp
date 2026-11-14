#include "KickEngine.h"

#include <algorithm>
#include <cmath>
#include <ranges>

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
    std::ranges::for_each(forwardVoices_, [this](KickVoice& voice) { voice.prepare(sampleRate_); });
    std::ranges::for_each(reverseVoices_, [this](KickVoice& voice) { voice.prepare(sampleRate_); });
    stutter_.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);
    toneSmooth_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.012 * sampleRate_)));
    reset();
}

void KickEngine::reset() noexcept {
    fallbackPpq_ = 0.0;
    currentPeak_ = currentBeatPhase_ = 0.0f;
    std::ranges::for_each(forwardVoices_, [](KickVoice& voice) { voice.reset(); });
    std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
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
    reverseCcEnabled_ = highPassCcEnabled_ = lowPassCcEnabled_ = false;
    reverseArmed_ = false;
    lastPpqStep_ = 0.0;
    hasLastPpq_ = false;
}

bool KickEngine::isVoiceReversing() const noexcept {
    return std::ranges::any_of(reverseVoices_, [](const KickVoice& voice) { return voice.isActive(); });
}

void KickEngine::triggerNote(
    const EngineSettings& settings,
    double samplePpq,
    double bpm,
    double beatsPerBar,
    int midiNote,
    float velocity,
    float effectiveTailMs,
    bool reverseCue) noexcept {
    ++triggerSerial_;
    velocity = unit(velocity);
    KickSettings voiceSettings = settings.synth;
    if (effectiveTailMs > 0.0f) {
        const float attack = std::clamp(voiceSettings.attackMs, 0.05f, 10.0f);
        const float target = std::clamp(effectiveTailMs, 15.0f, hitLengthMs(settings.synth));
        voiceSettings.holdMs = std::min(std::clamp(voiceSettings.holdMs, 0.0f, 400.0f),
                                        std::max(0.0f, target - attack - 1.0f));
        voiceSettings.decayMs = std::clamp(target - attack - voiceSettings.holdMs, 1.0f, 1500.0f);
    }
    ReverseSyncMode reverseMode = settings.effects.reverseMode;
    if (reverseMode == ReverseSyncMode::Off && reverseCcEnabled_) reverseMode = ReverseSyncMode::BeatEnd;
    const bool reverse = shouldTriggerReverse(reverseMode, reverseCue, samplePpq, beatsPerBar)
        && settings.effects.reverseMix > 0.001f;
    const auto mode = reverseMode == ReverseSyncMode::Off ? ReverseSyncMode::BeatEnd : reverseMode;
    double endPpq = samplePpq;
    int length = 0;
    if (reverse) {
        endPpq = reverseTargetPpq(mode, samplePpq, beatsPerBar);
        length = computeSyncedReverseSamples(mode, samplePpq, bpm, beatsPerBar, sampleRate_);
    }
    float reverseLevel = 0.0f;
    if (reverse && length > 0) reverseLevel = unit(settings.effects.reverseMix);
    const float forwardLevel = reverse && length > 0 ? 1.0f - reverseLevel : 1.0f;

    std::ranges::for_each(std::views::iota(size_t{ 0 }, static_cast<size_t>(kNumSoundLayers)), [&](size_t i) {
        const auto& layer = settings.layers[i];
        if (!layer.enabled || layer.gain <= 0.0001f) {
            forwardVoices_[i].reset();
            reverseVoices_[i].reset();
            return;
        }
        if (reverseLevel > 0.001f) {
            reverseVoices_[i].startReverse(voiceSettings, midiNote, velocity, length, layer, reverseLevel);
        }
        if (forwardLevel > 0.001f) {
            forwardVoices_[i].start(voiceSettings, midiNote, velocity, layer, forwardLevel);
        }
        if (forwardLevel <= 0.001f) {
            forwardVoices_[i].reset();
        }
    });

    if (reverseLevel > 0.001f && length > 0) {
        reverseArmed_ = true;
        reverseStartPpq_ = samplePpq;
        reverseEndPpq_ = endPpq;
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
            std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
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
                case 20: reverseCcEnabled_ = value >= 0.5f; break;
                case 21: highPassCcEnabled_ = value >= 0.5f; break;
                case 22: lowPassCcEnabled_ = value >= 0.5f; break;
                case 120:
                    std::ranges::for_each(forwardVoices_, [](KickVoice& voice) { voice.reset(); });
                    std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
                    reverseArmed_ = false;
                    break;
                case 121:
                    midiVolume_ = midiExpression_ = midiBrightness_ = 1.0f;
                    reverseCcEnabled_ = highPassCcEnabled_ = lowPassCcEnabled_ = false;
                    break;
                default: break;
            }
        }

        if (autoActive && transport.isPlaying) {
            const auto event = sequencer_.stepSample(slot, autoSettings, hitLengthMs(settings.synth),
                bpm, beatsPerBar, sampleRate_, hasHostPpq, ppq);
            if (event.triggered)
                triggerNote(settings, ppq, bpm, beatsPerBar, kDefaultMidiNote, event.velocity,
                            event.effectiveTailMs, event.reverseCue);
        }

        for (const auto& trigger : midiTriggers) {
            if (trigger.sampleOffset != sample || trigger.velocity <= 0.0f) continue;
            const int note = trigger.audition ? kDefaultMidiNote : trigger.noteNumber;
            triggerNote(settings, ppq, bpm, beatsPerBar, note, trigger.velocity, -1.0f, false);
        }

        float raw = 0.0f;
        std::ranges::for_each(std::views::iota(size_t{ 0 }, static_cast<size_t>(kNumSoundLayers)), [&](size_t i) {
            raw += forwardVoices_[i].nextSample() + reverseVoices_[i].nextSample();
        });
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

        const bool highPassSync = settings.effects.highPassSyncEnabled || highPassCcEnabled_;
        const bool lowPassSync = settings.effects.lowPassSyncEnabled || lowPassCcEnabled_;
        const auto syncMotion = [&](int rateIndex) noexcept {
            const double grid = division(rateIndex);
            const double cycle = std::max(0.0, ppq) / grid;
            const float phase = static_cast<float>(cycle - std::floor(cycle));
            return 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * phase);
        };
        const float hpBase = settings.effects.highPassHz;
        const float lpBase = settings.effects.lowPassHz;
        const float hpCeiling = hpBase > ToneFilter::kHighPassBypassHz + 0.5f ? hpBase : 8000.0f;
        const float lpFloor = lpBase < ToneFilter::kLowPassBypassHz - 1.0f ? lpBase : 200.0f;
        float highPassTarget = hpBase;
        float lowPassTarget = lpBase;
        if (highPassSync)
            highPassTarget = ToneFilter::kHighPassBypassHz + syncMotion(settings.effects.highPassSyncDivision)
                * (hpCeiling - ToneFilter::kHighPassBypassHz);
        if (lowPassSync)
            lowPassTarget = ToneFilter::kLowPassBypassHz - syncMotion(settings.effects.lowPassSyncDivision)
                * (ToneFilter::kLowPassBypassHz - lpFloor);

        if (toneSnap_) {
            smoothedHighPassHz_ = highPassTarget;
            smoothedLowPassHz_ = lowPassTarget;
            toneSnap_ = false;
        } else {
            smoothedHighPassHz_ += toneSmooth_ * (highPassTarget - smoothedHighPassHz_);
            smoothedLowPassHz_ += toneSmooth_ * (lowPassTarget - smoothedLowPassHz_);
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
