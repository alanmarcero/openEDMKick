#pragma once

#include "AutoSequencer.h"
#include "ClickSample.h"
#include "Effects.h"
#include "KickSynth.h"
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
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
    int midiChannel = 1;
};

// Performance MIDI. Not stored in the session.
struct MidiCcEvent {
    int sampleOffset = 0;
    int controller = 0;
    int value = 0;
};

struct MidiNoteOff { int sampleOffset = 0; int noteNumber = 60; int midiChannel = 1; };

using MidiInputEvent = std::variant<MidiTrigger, MidiCcEvent, MidiNoteOff>;

struct EffectSettings {
    ReverseSyncMode reverseMode = ReverseSyncMode::Off;
    float reverseMix = 1.0f;
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
    bool highPassSyncEnabled = false;
    int highPassSyncDivision = 2;
    bool lowPassSyncEnabled = false;
    int lowPassSyncDivision = 2;
    bool reverseEnabled = false;
    bool crushEnabled = false;
    bool stutterEnabled = false;
    bool gateEnabled = false;
    float midiVolume = 1;
    float midiExpression = 1;
    float midiBrightness = 1;
    bool highPassMidi = false;
    bool lowPassMidi = false;
    float stereoWidth = 0.0f;
    ThreeBandEqSettings eq{};
};

struct EngineSettings {
    KickSettings synth{};
    std::array<DrumLayerSettings, kNumSoundLayers> layers{{
        { true, DrumSoundType::kick, 1.0f, 0.0f, 300.0f },
        { false, DrumSoundType::snare, 0.65f, 12.0f, 220.0f },
        { false, DrumSoundType::clap, 0.5f, 0.0f, 140.0f },
        { false, DrumSoundType::hat, 0.35f, 0.0f, 90.0f }
    }};
    bool autoEnabled = false;
    bool autoBypass = false;
    int activeAutoSlot = 0; // 0=A, 1=B, 2=C, 3=D
    std::array<AutoSlotSettings, kNumAutoSlots> autoSlots{};
    EffectSettings effects{};
    bool masterBypass = false;
    double manualBpm = 0;
    double manualBeatsPerBar = 0;
    struct ClickSampleSettings {
        bool enabled = false;
        float level = 0.75f;
        float start = 0.0f;
        float pitchSemitones = 0.0f;
        float highPassHz = 20.0f;
    };
    ClickSampleSettings clickSample{};
};

struct MidiRouting {
    void* context = nullptr;
    void (*apply)(void*, int, int, EngineSettings&) noexcept = nullptr;
    void (*save)(void*) noexcept = nullptr;
    void (*restore)(void*, EngineSettings&) noexcept = nullptr;
    void (*beforeAuto)(void*, const EngineSettings&) noexcept = nullptr;
};

class KickEngine {
public:
    static constexpr int kLatencySamples = PeakLimiter::kLatency;

    void prepare(double sampleRate);
    void reset() noexcept;
    void setClickSampleSource(const float* samples, int length, double sampleRate) noexcept;
    void startClickSample(const EngineSettings& settings, float velocity) noexcept;

    // Each input span must be sorted by sampleOffset, retaining input order for ties.
    void process(
        const EngineSettings& settings,
        const Transport& transport,
        std::span<const MidiTrigger> midiTriggers,
        float* const* channels,
        int numChannels,
        int numSamples,
        std::span<const MidiCcEvent> controlChanges = {},
        std::span<const MidiInputEvent> midiEvents = {}, MidiRouting routing = {}) noexcept;

    [[nodiscard]] AutoSequencer& sequencer() noexcept { return sequencer_; }
    [[nodiscard]] const AutoSequencer& sequencer() const noexcept { return sequencer_; }

    [[nodiscard]] double currentBpm() const noexcept { return currentBpm_; }
    [[nodiscard]] float currentPeak() const noexcept { return currentPeak_; }
    [[nodiscard]] float currentAutoPhase() const noexcept { return sequencer_.currentWindowPhase(); }
    [[nodiscard]] float currentBeatPhase() const noexcept { return currentBeatPhase_; }
    [[nodiscard]] bool isVoiceReversing() const noexcept;
    [[nodiscard]] bool isStuttering() const noexcept { return stutterArmed_; }
    [[nodiscard]] int activeNoteCount() const noexcept;
    [[nodiscard]] double remainingTailSeconds() const noexcept;
    [[nodiscard]] uint32_t triggerSerial() const noexcept { return triggerSerial_; }

private:
    void synchronizeAmpMode(bool) noexcept;
    void triggerNote(
        const EngineSettings& settings,
        double samplePpq,
        double bpm,
        double beatsPerBar,
        int midiNote,
        float velocity,
        float effectiveTailMs,
        bool reverseCue, int onlyLayer = -1, int midiChannel = 0) noexcept;

    double sampleRate_ = 48000.0;
    double currentBpm_ = 128;
    double previousReverseBpm_ = 0;
    double effectTailAllowance_ = 0;
    int64_t effectTailRemaining_ = 0;
    double fallbackPpq_ = 0.0;
    float currentPeak_ = 0.0f;
    float currentBeatPhase_ = 0.0f;

    static constexpr int kPolyphony = 9;
    std::array<int, kPolyphony> voiceNotes_{};
    std::array<int, kPolyphony> voiceChannels_{};
    std::array<uint64_t, kPolyphony> voiceAges_{};
    uint64_t voiceCounter_ = 0;
    bool ampWasEnabled_ = false;
    std::vector<KickVoice> forwardVoices_{ static_cast<size_t>(kNumSoundLayers * kPolyphony) };
    std::vector<KickVoice> reverseVoices_{ static_cast<size_t>(kNumSoundLayers * kPolyphony) };
    AutoSequencer sequencer_;
    std::array<BitCrusher, 2> crusher_;
    std::array<BeatStutter, 2> stutter_;
    std::array<SyncFilterPump, 2> filterPump_;
    std::array<ToneFilter, 2> toneFilter_;
    struct ThiranSchroederStage {
        std::vector<float> buffer;
        size_t writeIndex = 0;
        size_t intDelay = 1;
        float thiranEta = 0.0f;
        float thiranX1 = 0.0f;
        float thiranY1 = 0.0f;
        float feedback = 0.5f;

        void reset() noexcept {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            writeIndex = 0;
            thiranX1 = 0.0f;
            thiranY1 = 0.0f;
        }

        [[nodiscard]] float process(float input) noexcept {
            if (buffer.empty()) return input;
            const size_t size = buffer.size();
            const size_t readIdx = (writeIndex + size - intDelay) % size;
            const float intDelayed = buffer[readIdx];
            const float fracDelayed = thiranEta * (intDelayed - thiranY1) + thiranX1;
            thiranX1 = intDelayed;
            thiranY1 = fracDelayed;
            const float v = input + feedback * fracDelayed;
            buffer[writeIndex] = std::isfinite(v) ? v : 0.0f;
            writeIndex = (writeIndex + 1u) % size;
            return fracDelayed - feedback * v;
        }
    };

    ThreeBandEq eq_;
    PeakLimiter limiter_;
    ClickSamplePlayer clickPlayer_;
    std::array<dsp::Svf, 2> widthMidLp_{};
    std::array<dsp::Svf, 2> widthMidHp_{};
    std::array<dsp::Svf, 2> widthSideLp_{};
    std::array<dsp::Svf, 2> widthSideHp_{};
    std::array<ThiranSchroederStage, 4> widthAllpass_{};
    float smoothedHighPassHz_ = ToneFilter::kHighPassBypassHz;
    float smoothedLowPassHz_ = ToneFilter::kLowPassBypassHz;
    float toneSmooth_ = 0.0f;
    bool toneSnap_ = true;
    float midiVolume_ = 1.0f;
    float midiExpression_ = 1.0f;
    float midiBrightness_ = 1.0f;
    bool reverseCcEnabled_ = false;
    bool highPassCcEnabled_ = false;
    bool lowPassCcEnabled_ = false;
    uint32_t triggerSerial_ = 0;
    bool crushCcEnabled_ = false;
    bool stutterCcEnabled_ = false;
    bool gateCcEnabled_ = false;
    bool stutterArmed_ = false;
    double stutterEndPpq_ = 0;
    bool reverseArmed_ = false;
    double reverseStartPpq_ = 0.0;
    double reverseEndPpq_ = 0.0;
    double lastSamplePpq_ = 0.0;
    double lastPpqStep_ = 0.0;
    bool hasLastPpq_ = false;
};

} // namespace openedmkick::core
