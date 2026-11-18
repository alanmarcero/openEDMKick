#include "KickEngine.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <type_traits>

namespace openedmkick::core {
namespace {

[[nodiscard]] float unit(float value) noexcept {
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
}

[[nodiscard]] float activationAmount(float amount, bool active) noexcept {
    const float normalized = unit(amount);
    if (active && normalized <= 0) return 1;
    return normalized;
}

[[nodiscard]] double safeTempo(double bpm) noexcept {
    return std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
}

} // namespace

void KickEngine::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    std::ranges::for_each(forwardVoices_, [this](KickVoice& voice) { voice.prepare(sampleRate_); });
    std::ranges::for_each(reverseVoices_, [this](KickVoice& voice) { voice.prepare(sampleRate_); });
    for (auto& effect : stutter_) effect.prepare(sampleRate_);
    limiter_.prepare(sampleRate_);
    widthDelayBuffer_.assign(std::max<size_t>(16, static_cast<size_t>(sampleRate_ * 0.015)), 0.0f);
    toneSmooth_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.012 * sampleRate_)));
    reset();
}

void KickEngine::reset() noexcept {
    fallbackPpq_ = 0.0;
    effectTailAllowance_ = 0;
    effectTailRemaining_ = 0;
    currentPeak_ = currentBeatPhase_ = 0.0f;
    std::ranges::for_each(forwardVoices_, [](KickVoice& voice) { voice.reset(); });
    std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
    sequencer_.reset();
    for (auto& effect : crusher_) effect.reset();
    for (auto& effect : stutter_) effect.reset();
    for (auto& effect : filterPump_) effect.reset();
    for (auto& effect : toneFilter_) effect.reset();
    limiter_.reset();
    std::fill(widthDelayBuffer_.begin(), widthDelayBuffer_.end(), 0.0f);
    widthWriteIndex_ = 0;
    widthLowPassState_ = 0.0f;
    smoothedHighPassHz_ = ToneFilter::kHighPassBypassHz;
    smoothedLowPassHz_ = ToneFilter::kLowPassBypassHz;
    toneSnap_ = true;
    midiVolume_ = midiExpression_ = midiBrightness_ = 1.0f;
    reverseCcEnabled_ = highPassCcEnabled_ = lowPassCcEnabled_ = false;
    reverseArmed_ = stutterArmed_ = false;
    crushCcEnabled_ = stutterCcEnabled_ = gateCcEnabled_ = false;
    lastPpqStep_ = 0.0;
    hasLastPpq_ = false;
}

int KickEngine::activeNoteCount() const noexcept {
    int count = 0;
    for (size_t slot = 0; slot < kPolyphony; ++slot) {
        bool active = false;
        for (size_t layer = 0; layer < kNumSoundLayers; ++layer)
            active = active || forwardVoices_[slot * kNumSoundLayers + layer].isActive() || reverseVoices_[slot * kNumSoundLayers + layer].isActive();
        if (active) ++count;
    }
    return count;
}

double KickEngine::remainingTailSeconds() const noexcept {
    double remaining = 0;
    bool active = false;
    for (const auto& voice : forwardVoices_) { remaining = std::max(remaining, voice.remainingSeconds()); active = active || voice.isActive(); }
    for (const auto& voice : reverseVoices_) { remaining = std::max(remaining, voice.remainingSeconds()); active = active || voice.isActive(); }
    if (stutterArmed_) {
        remaining = std::max(remaining, std::max(0.0, stutterEndPpq_ - lastSamplePpq_) * 60.0 / currentBpm_);
        active = true;
    }
    if (active) remaining += effectTailAllowance_;
    return std::max(remaining, static_cast<double>(effectTailRemaining_) / sampleRate_);
}

bool KickEngine::isVoiceReversing() const noexcept {
    return std::ranges::any_of(reverseVoices_, [](const KickVoice& voice) { return voice.isActive(); });
}

void KickEngine::synchronizeAmpMode(bool enabled) noexcept {
    if (enabled == ampWasEnabled_) return;
    if (!enabled) {
        for (size_t index = kNumSoundLayers; index < forwardVoices_.size(); ++index) {
            forwardVoices_[index].release();
            reverseVoices_[index].release();
        }
    }
    ampWasEnabled_ = enabled;
}

void KickEngine::triggerNote(
    const EngineSettings& settings,
    double samplePpq,
    double bpm,
    double beatsPerBar,
    int midiNote,
    float velocity,
    float effectiveTailMs,
    bool reverseCue, int onlyLayer, int midiChannel) noexcept {
    synchronizeAmpMode(settings.synth.ampEnabled);
    velocity = unit(velocity);
    KickSettings voiceSettings = settings.synth;
    voiceSettings.ampEnabled = voiceSettings.ampEnabled && midiChannel > 0;
    size_t voiceSlot = 0;
    if (voiceSettings.ampEnabled) {
        const auto active = [&](size_t slot) {
            for (size_t layer = 0; layer < kNumSoundLayers; ++layer)
                if (forwardVoices_[slot * kNumSoundLayers + layer].isActive() || reverseVoices_[slot * kNumSoundLayers + layer].isActive()) return true;
            return false;
        };
        bool found = false;
        for (size_t slot = 1; slot < kPolyphony; ++slot) {
            if (active(slot) && voiceNotes_[slot] == midiNote && voiceChannels_[slot] == midiChannel) { voiceSlot = slot; found = true; break; }
        }
        if (!found) {
            for (size_t slot = 1; slot < kPolyphony; ++slot) {
                if (!active(slot)) { voiceSlot = slot; found = true; break; }
            }
        }
        if (!found) voiceSlot = static_cast<size_t>(std::min_element(voiceAges_.begin() + 1, voiceAges_.end()) - voiceAges_.begin());
        for (size_t layer = 0; layer < kNumSoundLayers; ++layer) {
            forwardVoices_[voiceSlot * kNumSoundLayers + layer].reset();
            reverseVoices_[voiceSlot * kNumSoundLayers + layer].reset();
        }
        voiceNotes_[voiceSlot] = midiNote;
        voiceChannels_[voiceSlot] = midiChannel;
        voiceAges_[voiceSlot] = ++voiceCounter_;
    }
    if (stutterCcEnabled_ || settings.effects.stutterEnabled || settings.effects.stutterMix > 0) {
        stutterArmed_ = true;
        stutterEndPpq_ = reverseTargetPpq(ReverseSyncMode::BeatEnd, samplePpq, beatsPerBar);
    }
    if (effectiveTailMs > 0.0f) {
        const float attack = std::clamp(voiceSettings.attackMs, 0.05f, 10.0f);
        const float target = std::clamp(effectiveTailMs, 15.0f, hitLengthMs(settings.synth));
        voiceSettings.holdMs = std::min(std::clamp(voiceSettings.holdMs, 0.0f, 400.0f),
                                        std::max(0.0f, target - attack - 1.0f));
        voiceSettings.decayMs = std::clamp(target - attack - voiceSettings.holdMs, 1.0f, 1500.0f);
    }
    ReverseSyncMode reverseMode = settings.effects.reverseMode;
    if (reverseCcEnabled_ || settings.effects.reverseEnabled) reverseMode = ReverseSyncMode::BeatEnd;
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
        if (onlyLayer >= 0 && i != static_cast<size_t>(onlyLayer)) return;
        const auto& layer = settings.layers[i];
        const size_t voiceIndex = voiceSlot * kNumSoundLayers + i;
        if (!layer.enabled || layer.gain <= 0.0001f) {
            forwardVoices_[voiceIndex].reset();
            reverseVoices_[voiceIndex].reset();
            return;
        }
        const int midiVelocity = std::clamp(static_cast<int>(std::lround(velocity * 127.0f)), 1, 127);
        if (midiNote < layer.keyLow || midiNote > layer.keyHigh
            || midiVelocity < layer.velocityLow || midiVelocity > layer.velocityHigh) return;
        if (reverseLevel > 0.001f) {
            reverseVoices_[voiceIndex].startReverse(voiceSettings, midiNote, velocity, length, layer, reverseLevel);
        }
        if (forwardLevel > 0.001f) {
            forwardVoices_[voiceIndex].start(voiceSettings, midiNote, velocity, layer, forwardLevel);
        }
        if (forwardLevel <= 0.001f) {
            forwardVoices_[voiceIndex].reset();
        }
    });

    if (reverseLevel > 0.001f && length > 0) {
        reverseArmed_ = true;
        reverseStartPpq_ = samplePpq;
        reverseEndPpq_ = endPpq;
    }
}

void KickEngine::process(
    const EngineSettings& inputSettings,
    const Transport& transport,
    std::span<const MidiTrigger> midiTriggers,
    float* const* channels,
    int numChannels,
    int numSamples,
    std::span<const MidiCcEvent> controlChanges,
    std::span<const MidiInputEvent> midiEvents, MidiRouting routing) noexcept {
    EngineSettings liveSettings = inputSettings;
    const EngineSettings& settings = liveSettings;
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0) return;

    synchronizeAmpMode(settings.synth.ampEnabled);
    const double hostBpm = safeTempo(transport.bpm);
    const double hostBeatsPerBar = std::clamp(std::isfinite(transport.beatsPerBar) ? transport.beatsPerBar : 4.0, 1.0, 16.0);
    const bool hostPpqAvailable = transport.ppqPosition.has_value() && std::isfinite(*transport.ppqPosition);
    double ppqCursor = hostPpqAvailable && settings.manualBpm <= 0 ? *transport.ppqPosition : fallbackPpq_;
    const auto division = [](int index) noexcept {
        return kSyncDivisionBeats[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kSyncDivisionBeats.size()) - 1))];
    };
    float peak = 0.0f;
    size_t midiCursor = 0;
    size_t ccCursor = 0;
    size_t noteCursor = 0;
    const auto eventOffset = [](const MidiInputEvent& event) noexcept {
        return std::visit([](const auto& value) { return value.sampleOffset; }, event);
    };
    int nextMidiSample = midiEvents.empty() ? numSamples : eventOffset(midiEvents.front());
    int nextCcSample = controlChanges.empty() ? numSamples : controlChanges.front().sampleOffset;
    int nextNoteSample = midiTriggers.empty() ? numSamples : midiTriggers.front().sampleOffset;

    for (int sample = 0; sample < numSamples; ++sample) {
        double bpm = settings.manualBpm > 0 ? safeTempo(settings.manualBpm) : hostBpm;
        double beatsPerBar = settings.manualBeatsPerBar > 0 ? settings.manualBeatsPerBar : hostBeatsPerBar;
        double ppqPerSample = bpm / (60.0 * sampleRate_);
        const bool hostClock = hostPpqAvailable && settings.manualBpm <= 0;
        const double ppq = hostClock ? *transport.ppqPosition + static_cast<double>(sample) * hostBpm / (60.0 * sampleRate_) : ppqCursor;
        currentBeatPhase_ = static_cast<float>(ppq - std::floor(ppq));

        const double expectedPpqStep = lastPpqStep_ > 0.0 ? lastPpqStep_ : ppqPerSample;
        const double seekTolerance = std::max(1.0e-7, expectedPpqStep * 0.5);
        const bool seekedBack = hasLastPpq_ && ppq + 1.0e-7 < lastSamplePpq_;
        const bool seekedForward = hasLastPpq_ && ppq > lastSamplePpq_ + expectedPpqStep + seekTolerance;
        if (reverseArmed_ && ((!transport.isPlaying && settings.manualBpm <= 0) || seekedBack || seekedForward || ppq >= reverseEndPpq_)) {
            std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
            reverseArmed_ = false;
        }
        if (stutterArmed_ && ((!transport.isPlaying && settings.manualBpm <= 0) || seekedBack || seekedForward || ppq >= stutterEndPpq_)) stutterArmed_ = false;
        hasLastPpq_ = true;
        lastSamplePpq_ = ppq;
        lastPpqStep_ = ppqPerSample;

        const auto applyControlChange = [&](const MidiCcEvent& cc) {
            if (cc.sampleOffset != sample) return;
            if (routing.apply != nullptr) {
                routing.apply(routing.context, cc.controller, cc.value, liveSettings);
                if (cc.controller != 120 && cc.controller != 121) return;
            }
            const float value = std::clamp(cc.value, 0, 127) / 127.0f;
            switch (cc.controller) {
                case 7: midiVolume_ = value; break;
                case 11: midiExpression_ = value; break;
                case 74: midiBrightness_ = value; break;
                case 20: reverseCcEnabled_ = value >= 0.5f; break;
                case 21: highPassCcEnabled_ = value >= 0.5f; break;
                case 22: lowPassCcEnabled_ = value >= 0.5f; break;
                case 23: crushCcEnabled_ = value >= 0.5f; break;
                case 24: stutterCcEnabled_ = value >= 0.5f; break;
                case 25: gateCcEnabled_ = value >= 0.5f; break;
                case 120:
                    std::ranges::for_each(forwardVoices_, [](KickVoice& voice) { voice.reset(); });
                    std::ranges::for_each(reverseVoices_, [](KickVoice& voice) { voice.reset(); });
                    reverseArmed_ = stutterArmed_ = false;
                    break;
                case 121:
                    midiVolume_ = midiExpression_ = midiBrightness_ = 1.0f;
                    reverseCcEnabled_ = highPassCcEnabled_ = lowPassCcEnabled_ = false;
                    crushCcEnabled_ = stutterCcEnabled_ = gateCcEnabled_ = false;
                    break;
                default: break;
            }
        };

        const auto triggerMidiNote = [&](const MidiTrigger& trigger) {
            if (trigger.sampleOffset != sample || trigger.velocity <= 0.0f) return;
            const int note = trigger.audition ? kDefaultMidiNote : trigger.noteNumber;
            ++triggerSerial_;
            const double noteBpm = settings.manualBpm > 0 ? safeTempo(settings.manualBpm) : hostBpm;
            const double noteMeter = settings.manualBeatsPerBar > 0 ? settings.manualBeatsPerBar : hostBeatsPerBar;
            triggerNote(settings, ppq, noteBpm, noteMeter, note, trigger.velocity, -1.0f, false, -1, trigger.audition ? 0 : trigger.midiChannel);
        };

        const float savedMidiVolume = midiVolume_;
        const float savedMidiExpression = midiExpression_;
        const float savedMidiBrightness = midiBrightness_;
        const bool savedCrushCc = crushCcEnabled_;
        const bool savedStutterCc = stutterCcEnabled_;
        const bool savedGateCc = gateCcEnabled_;
        const bool savedReverseCcEnabled = reverseCcEnabled_;
        const bool savedHighPassCcEnabled = highPassCcEnabled_;
        const bool savedLowPassCcEnabled = lowPassCcEnabled_;

        const size_t midiBegin = midiCursor;
        while (midiCursor < midiEvents.size() && nextMidiSample <= sample) {
            ++midiCursor;
            if (midiCursor < midiEvents.size()) nextMidiSample = eventOffset(midiEvents[midiCursor]);
        }
        const auto sampleMidiEvents = midiEvents.subspan(midiBegin, midiCursor - midiBegin);
        if (midiEvents.empty()) {
            while (ccCursor < controlChanges.size() && nextCcSample <= sample) {
                applyControlChange(controlChanges[ccCursor]);
                ++ccCursor;
                if (ccCursor < controlChanges.size()) nextCcSample = controlChanges[ccCursor].sampleOffset;
            }
        }
        if (!sampleMidiEvents.empty()) {
            if (routing.save != nullptr) routing.save(routing.context);
            // Auto Mode sees all controllers at this sample; incoming notes retain their original CC order below.
            std::ranges::for_each(sampleMidiEvents, [&](const MidiInputEvent& inputEvent) {
                std::visit([&](const auto& event) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(event)>, MidiCcEvent>)
                        applyControlChange(event);
                }, inputEvent);
            });
        }

        bpm = settings.manualBpm > 0 ? safeTempo(settings.manualBpm) : hostBpm;
        beatsPerBar = settings.manualBeatsPerBar > 0 ? settings.manualBeatsPerBar : hostBeatsPerBar;
        ppqPerSample = bpm / (60.0 * sampleRate_);
        currentBpm_ = bpm;
        if (routing.beforeAuto != nullptr && !sampleMidiEvents.empty()) routing.beforeAuto(routing.context, settings);
        const bool autoActive = settings.autoEnabled && !settings.autoBypass;
        const int slot = std::clamp(settings.activeAutoSlot, 0, kNumAutoSlots - 1);
        const auto& autoSettings = settings.autoSlots[static_cast<size_t>(slot)];
        if (autoActive && (transport.isPlaying || settings.manualBpm > 0)) {
            const auto event = sequencer_.stepSample(slot, autoSettings, hitLengthMs(settings.synth),
                bpm, beatsPerBar, sampleRate_, true, ppq);
            if (event.triggered) {
                ++triggerSerial_;
                for (int layer = 0; layer < kNumSoundLayers; ++layer) {
                    if ((event.layerMask & (1u << layer)) == 0) continue;
                    const auto index = static_cast<size_t>(layer);
                    triggerNote(settings, ppq, bpm, beatsPerBar, kDefaultMidiNote, event.layerVelocities[index],
                                event.layerTailMs[index], event.layerReverseCue[index], layer);
                }
            }
        }

        if (midiEvents.empty()) {
            while (noteCursor < midiTriggers.size() && nextNoteSample <= sample) {
                triggerMidiNote(midiTriggers[noteCursor]);
                ++noteCursor;
                if (noteCursor < midiTriggers.size()) nextNoteSample = midiTriggers[noteCursor].sampleOffset;
            }
        }
        if (!sampleMidiEvents.empty()) {
            if (routing.restore != nullptr) routing.restore(routing.context, liveSettings);
            midiVolume_ = savedMidiVolume;
            midiExpression_ = savedMidiExpression;
            midiBrightness_ = savedMidiBrightness;
            crushCcEnabled_ = savedCrushCc;
            stutterCcEnabled_ = savedStutterCc;
            gateCcEnabled_ = savedGateCc;
            reverseCcEnabled_ = savedReverseCcEnabled;
            highPassCcEnabled_ = savedHighPassCcEnabled;
            lowPassCcEnabled_ = savedLowPassCcEnabled;
            std::ranges::for_each(sampleMidiEvents, [&](const MidiInputEvent& inputEvent) {
                std::visit([&](const auto& event) {
                    if (event.sampleOffset != sample) return;
                    using Event = std::decay_t<decltype(event)>;
                    if constexpr (std::is_same_v<Event, MidiCcEvent>) {
                        if (event.controller != 120) applyControlChange(event);
                        if (event.controller == 120 && routing.apply != nullptr)
                            routing.apply(routing.context, event.controller, event.value, liveSettings);
                    }
                    if constexpr (std::is_same_v<Event, MidiTrigger>) triggerMidiNote(event);
                    if constexpr (std::is_same_v<Event, MidiNoteOff>) {
                        for (size_t slot = 0; slot < kPolyphony; ++slot) {
                            if (voiceNotes_[slot] != event.noteNumber || voiceChannels_[slot] != event.midiChannel) continue;
                            for (size_t layer = 0; layer < kNumSoundLayers; ++layer) {
                                forwardVoices_[slot * kNumSoundLayers + layer].release();
                                reverseVoices_[slot * kNumSoundLayers + layer].release();
                            }
                        }
                    }
                }, inputEvent);
            });
        }

        synchronizeAmpMode(settings.synth.ampEnabled);
        const double finalBpm = settings.manualBpm > 0 ? safeTempo(settings.manualBpm) : hostBpm;
        ppqCursor = ppq + finalBpm / (60.0 * sampleRate_);
        currentBpm_ = finalBpm;
        lastPpqStep_ = finalBpm / (60.0 * sampleRate_);
        if (reverseArmed_ && std::abs(finalBpm - previousReverseBpm_) > 1.0e-8) {
            const int remaining = samplesUntilPpq(ppq, reverseEndPpq_, finalBpm, sampleRate_);
            for (auto& voice : reverseVoices_) voice.retimeReverse(remaining);
        }
        previousReverseBpm_ = finalBpm;
        std::array<float, 2> raw{};
        const bool anySolo = std::ranges::any_of(settings.layers, [](const auto& layer) { return layer.enabled && layer.solo; });
        for (size_t i = 0; i < forwardVoices_.size(); ++i) {
            const float voice = forwardVoices_[i].nextSample() + reverseVoices_[i].nextSample();
            const auto& layer = settings.layers[i % kNumSoundLayers];
            if (!layer.enabled || (anySolo && !layer.solo)) continue;
            const float signedVoice = layer.invertPolarity ? -voice : voice;
            const float pan = std::clamp(std::isfinite(layer.pan) ? layer.pan : 0.0f, -1.0f, 1.0f);
            raw[0] += signedVoice * (1.0f - std::max(0.0f, pan));
            raw[1] += signedVoice * (1.0f + std::min(0.0f, pan));
        }
        const bool highPassSync = settings.effects.highPassSyncEnabled || highPassCcEnabled_ || settings.effects.highPassMidi;
        const bool lowPassSync = settings.effects.lowPassSyncEnabled || lowPassCcEnabled_ || settings.effects.lowPassMidi;
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
        const float brightnessMax = 40.0f + midiBrightness_ * settings.effects.midiBrightness * (ToneFilter::kLowPassBypassHz - 40.0f);
        const bool filterMemory = smoothedHighPassHz_ > ToneFilter::kHighPassBypassHz + 0.5f
            || std::min(smoothedLowPassHz_, brightnessMax) < ToneFilter::kLowPassBypassHz - 1
            || settings.effects.pumpMix > 0;
        const bool crushMemory = settings.effects.crushMix > 0 || settings.effects.crushEnabled || crushCcEnabled_
            || (autoActive && autoSettings.crushSweep > 0);
        const double crusherRate = std::clamp(std::isfinite(settings.effects.crushRateHz) ? settings.effects.crushRateHz : 8000.0f, 100.0f, static_cast<float>(sampleRate_));
        effectTailAllowance_ = filterMemory ? 0.5 : (crushMemory ? 2.0 / crusherRate : 0);
        for (size_t side = 0; side < static_cast<size_t>(std::min(2, numChannels)); ++side) {
            const float input = numChannels == 1 ? (raw[0] + raw[1]) * 0.5f : raw[side];
            const float driven = saturate(input, settings.effects.drive);
            const float ramp = autoActive ? unit(autoSettings.crushSweep) * sequencer_.currentWindowPhase() : 0.0f;
            const float crushMix = std::clamp(activationAmount(settings.effects.crushMix, crushCcEnabled_ || settings.effects.crushEnabled) + ramp, 0.0f, 1.0f);
            const float crushed = crusher_[side].process(driven, settings.effects.crushRateHz, settings.effects.crushBits, sampleRate_);
            const float postCrush = driven + crushMix * (crushed - driven);
            const float repeated = stutter_[side].process(postCrush, ppq, division(settings.effects.stutterDivision), bpm, sampleRate_);
            const float postRepeat = postCrush + (stutterArmed_ ? activationAmount(settings.effects.stutterMix, stutterCcEnabled_ || settings.effects.stutterEnabled) : 0.0f) * (repeated - postCrush);
            const float pumped = filterPump_[side].process(postRepeat, ppq, division(settings.effects.pumpDivision),
                                                      settings.effects.pumpMix, sampleRate_);
            const float gated = pumped * TempoGate::gainAt(ppq, division(settings.effects.gateDivision), activationAmount(settings.effects.gateMix, gateCcEnabled_ || settings.effects.gateEnabled));
            float out = gated * settings.effects.outputGain * midiVolume_ * midiExpression_ * settings.effects.midiVolume * settings.effects.midiExpression;

            if (std::abs(out) > 1.0e-8f && effectTailAllowance_ > 0)
                effectTailRemaining_ = static_cast<int64_t>(std::ceil(effectTailAllowance_ * sampleRate_));
            out = toneFilter_[side].process(out, smoothedHighPassHz_, std::min(smoothedLowPassHz_, brightnessMax), sampleRate_);
            if (!std::isfinite(out) || settings.masterBypass) out = 0.0f;
            peak = std::max(peak, std::abs(out));
            channels[side][sample] = out;
        }
        if (numChannels > 1 && !widthDelayBuffer_.empty() && !settings.masterBypass) {
            const float monoMid = 0.5f * (channels[0][sample] + channels[1][sample]);
            const float lpCoeff = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * 450.0 / sampleRate_));
            widthLowPassState_ += lpCoeff * (monoMid - widthLowPassState_);
            const float highBand = monoMid - widthLowPassState_;
            widthDelayBuffer_[widthWriteIndex_] = highBand;
            const float widthAmount = unit(settings.effects.stereoWidth);
            if (widthAmount > 0.001f) {
                const size_t delayTap = std::clamp<size_t>(static_cast<size_t>(sampleRate_ * 0.0065), 1, widthDelayBuffer_.size() - 1);
                const float delayedHigh = widthDelayBuffer_[(widthWriteIndex_ + widthDelayBuffer_.size() - delayTap) % widthDelayBuffer_.size()];
                const float sideSignal = delayedHigh * widthAmount * 0.55f;
                channels[0][sample] += sideSignal;
                channels[1][sample] -= sideSignal;
                peak = std::max({peak, std::abs(channels[0][sample]), std::abs(channels[1][sample])});
            }
            widthWriteIndex_ = (widthWriteIndex_ + 1) % widthDelayBuffer_.size();
        }
        if (effectTailRemaining_ > 0) --effectTailRemaining_;
        if (settings.effects.limiterEnabled) {
            const std::array<float*, 2> frame{channels[0] + sample, numChannels > 1 ? channels[1] + sample : nullptr};
            limiter_.process(frame.data(), std::min(2, numChannels), 1, settings.effects.ceilingGain);
        }
    }

    if (transport.isPlaying || settings.manualBpm > 0) fallbackPpq_ = ppqCursor;
    currentPeak_ = std::max(peak, currentPeak_ * 0.82f);
}

} // namespace openedmkick::core
