#pragma once

#include "AutoSequencer.h"
#include "Effects.h"
#include "KickSynth.h"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace openedmkick::core {

struct Transport {
    double bpm = 128.0;
    std::optional<double> ppqPosition;
    double beatsPerBar = 4.0;
    bool isPlaying = true;
};

struct MidiTrigger {
    int sampleOffset = 0;
    int noteNumber = 60;
    float velocity = 1.0f;
    bool audition = false;
};

// Performance MIDI. Not stored in the session.
struct MidiCcEvent {
    int sampleOffset = 0;
    int controller = 0;
    int value = 0;
};

struct EffectSettings {
    ReverseSyncMode reverseMode = ReverseSyncMode::Off;
    float reverseMix = 1.0f;
    float drive = 0.15f;
    float crushRateHz = 8000.0f;
    int crushBits = 8;
    float crushMix = 0.0f;
    int stutterDivision = 2; // index into kSyncDivisionBeats (2 = 1/16)
    float stutterMix = 0.0f;
    int gateDivision = 2;    // index into kSyncDivisionBeats (2 = 1/16)
    float gateMix = 0.0f;
    int pumpDivision = 0;    // index into kSyncDivisionBeats (0 = 1/4)
    float pumpMix = 0.0f;
    float outputGain = 1.0f;
    bool limiterEnabled = true;
    float ceilingGain = 0.966f;
    float highPassHz = ToneFilter::kHighPassBypassHz;
    float lowPassHz = ToneFilter::kLowPassBypassHz;
};

struct EngineSettings {
    KickSynthSettings synth{};
    bool lockIn = false;
    bool autoEnabled = false;
    bool autoBypass = false;
    int activeAutoSlot = 0; // 0=A, 1=B, 2=C, 3=D
    bool noteTracking = false;
    std::array<AutoSlotSettings, kNumAutoSlots> autoSlots{};
    EffectSettings effects{};
    bool masterBypass = false;
};

class KickEngine {
public:
    static constexpr int kLatencySamples = 0;

    void prepare(double sampleRate);
    void reset() noexcept;

    void process(
        const EngineSettings& settings,
        const Transport& transport,
        std::span<const MidiTrigger> midiTriggers,
        float* const* channels,
        int numChannels,
        int numSamples,
        std::span<const MidiCcEvent> controlChanges = {}) noexcept;

    void lockCurrentKick(const KickSynthSettings& settings);
    void unlockKick() noexcept;
    bool restoreLockedWav(const std::vector<uint8_t>& wavBytes);

    [[nodiscard]] bool isKickLocked() const noexcept { return wavCache_.isLocked(); }
    [[nodiscard]] const KickWavCache& wavCache() const noexcept { return wavCache_; }
    [[nodiscard]] AutoSequencer& sequencer() noexcept { return sequencer_; }
    [[nodiscard]] const AutoSequencer& sequencer() const noexcept { return sequencer_; }

    [[nodiscard]] float currentPeak() const noexcept { return currentPeak_; }
    [[nodiscard]] float currentAutoPhase() const noexcept { return sequencer_.currentWindowPhase(); }
    [[nodiscard]] float currentBeatPhase() const noexcept { return currentBeatPhase_; }
    [[nodiscard]] bool isVoiceReversing() const noexcept { return reverseVoice_.isActive(); }
    [[nodiscard]] uint32_t triggerSerial() const noexcept { return triggerSerial_; }

private:
    void triggerNote(
        const EngineSettings& settings,
        double samplePpq,
        double bpm,
        double beatsPerBar,
        float velocity,
        float semitoneOffset,
        float effectiveTailMs,
        bool reverseCue) noexcept;

    double sampleRate_ = 48000.0;
    double fallbackPpq_ = 0.0;
    bool previousLockState_ = false;
    float currentPeak_ = 0.0f;
    float currentBeatPhase_ = 0.0f;

    KickWavCache wavCache_;
    KickVoice forwardVoice_;
    KickVoice reverseVoice_;
    AutoSequencer sequencer_;
    BitCrusher crusher_;
    BeatStutter stutter_;
    SyncFilterPump filterPump_;
    ToneFilter toneFilter_;
    PeakLimiter limiter_;
    float smoothedHighPassHz_ = ToneFilter::kHighPassBypassHz;
    float smoothedLowPassHz_ = ToneFilter::kLowPassBypassHz;
    float toneSmooth_ = 0.0f;
    bool toneSnap_ = true;
    float midiVolume_ = 1.0f;
    float midiExpression_ = 1.0f;
    float midiBrightness_ = 1.0f;
    uint32_t triggerSerial_ = 0;
    bool reverseArmed_ = false;
    double reverseStartPpq_ = 0.0;
    double reverseEndPpq_ = 0.0;
    double lastSamplePpq_ = 0.0;
    double lastPpqStep_ = 0.0;
    bool hasLastPpq_ = false;
};

} // namespace openedmkick::core
