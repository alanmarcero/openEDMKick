#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace openedmkick::core {

inline constexpr int kNumNotes = 12;

inline constexpr std::array<std::string_view, kNumNotes> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

struct KickSynthSettings {
    int note = 5;              // 0..11 (C..B), default F (43.65 Hz at octave 1)
    int octave = 1;            // 0..2
    float fineCents = 0.0f;    // -50..+50 cents
    float startPitchHz = 420.0f;
    float pitchDecayMs = 42.0f;
    float pitchCurve = 0.25f;  // -1..+1
    float tailLengthMs = 380.0f;
    float tailCurve = 0.15f;   // -1..+1
    float subDrive = 0.25f;    // 0..1
    float clickLevel = 0.35f;  // 0..1
    float clickFreqHz = 2800.0f;
    float punch = 0.45f;       // 0..1
    // Optional timbre layers. Levels default to 0 so existing kicks stay unchanged.
    float snareLevel = 0.0f;   // 0..1
    float snareTone = 0.55f;   // 0 body .. 1 wires
    float snareDecay = 0.4f;   // 0 short .. 1 long
    float clapLevel = 0.0f;    // 0..1
    float clapSpread = 0.45f;  // 0 tight .. 1 wide flam
    float clapTone = 0.6f;     // 0 dark .. 1 bright
    float clapDecay = 0.35f;   // 0 short .. 1 long. 0.35 keeps the previous clap tail.
    float hatLevel = 0.0f;     // 0..1
    float hatTone = 0.7f;      // 0 chunky .. 1 thin
    float hatDecay = 0.25f;    // 0 closed .. 1 open
    float bodyTone = 0.65f;    // 0 round fundamental .. 1 bright harmonics
    float attackMs = 0.35f;
    float decayMs = 72.0f;
    float sustainLevel = 0.7f;
    float releaseMs = 180.0f;

    bool operator==(const KickSynthSettings&) const noexcept = default;
};

// Layer time mappings shared by the synth and the parameter readouts.
[[nodiscard]] inline float snareDecayMs(float amount) noexcept {
    const float clamped = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    return 28.0f + clamped * 210.0f;
}

[[nodiscard]] inline float clapSpreadMs(float amount) noexcept {
    const float clamped = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    return 4.0f + clamped * 11.0f;
}

[[nodiscard]] inline float hatDecayMs(float amount) noexcept {
    const float clamped = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    return 16.0f + clamped * 340.0f;
}

// 0.35 is unity, so a missing clap_decay parameter keeps the old tail.
// 0 is about 0.35x that tail. 1 is about 7x.
[[nodiscard]] inline float clapDecayScale(float amount) noexcept {
    const float clamped = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    return std::exp2((clamped - 0.35f) * 4.326f);
}

// Tail time constant at a given flam width. Spread still sets the gap between hits.
[[nodiscard]] inline float clapTailTimeMs(float decayAmount, float spread) noexcept {
    const float clampedSpread = spread < 0.0f ? 0.0f : (spread > 1.0f ? 1.0f : spread);
    return (16.0f + clampedSpread * 36.0f) * clapDecayScale(decayAmount);
}

// How long an active layer stays audible, in milliseconds. Zero when every layer is off,
// so a dry kick keeps the Tail Length exactly.
[[nodiscard]] inline float activeLayerHorizonMs(const KickSynthSettings& settings) noexcept {
    constexpr float kToSilence = 6.907755f;
    float horizon = 0.0f;
    if (settings.snareLevel > 0.0008f) {
        horizon = std::max(horizon, snareDecayMs(settings.snareDecay) * 0.78f * kToSilence);
    }
    if (settings.clapLevel > 0.0008f) {
        const float scale = clapDecayScale(settings.clapDecay);
        const float spreadMs = clapSpreadMs(settings.clapSpread);
        const float tail = spreadMs * 3.0f + clapTailTimeMs(settings.clapDecay, settings.clapSpread) * 5.8f;
        const float burst = 22.0f * scale + spreadMs * 3.0f;
        horizon = std::max(horizon, std::max(tail, burst));
    }
    if (settings.hatLevel > 0.0008f) {
        horizon = std::max(horizon, hatDecayMs(settings.hatDecay) * kToSilence);
    }
    return horizon > 2500.0f ? 2500.0f : horizon;
}

// MIDI note number for the panel tuning. C4 is 60.
// Oct 1 Club C is 24. Oct 1 Club F is 29. Oct 2 Punch C is 36.
[[nodiscard]] inline int midiRootForTuning(int noteIndex, int octave) noexcept {
    const int note = noteIndex >= 0 ? noteIndex % kNumNotes : (noteIndex % kNumNotes + kNumNotes) % kNumNotes;
    const int oct = octave < 0 ? 0 : (octave > 3 ? 3 : octave);
    return 12 + oct * 12 + note;
}

[[nodiscard]] inline float trackedSemitoneOffset(int noteIndex, int octave, int midiNote) noexcept {
    const int delta = midiNote - midiRootForTuning(noteIndex, octave);
    const int clamped = delta < -48 ? -48 : (delta > 48 ? 48 : delta);
    return static_cast<float>(clamped);
}

// Returns the tuned fundamental frequency in Hz for any of the 12 chromatic notes.
[[nodiscard]] float fundamentalHzForNote(int noteIndex, int octave = 1, float fineCents = 0.0f) noexcept;

// Shapes a normalized progress (0..1) with a bipolar curve (-1..+1).
[[nodiscard]] float shapeCurve(float progress, float curve) noexcept;

// Pitch-drop amount for the kick sweep. Returns 1 at the start of the decay
// and 0 once `progress` reaches 1. Positive curves fall faster.
[[nodiscard]] float pitchSweepGain(float progress, float pitchCurve) noexcept;

// Oscillator memory for one kick voice. Reset on each trigger so live synthesis
// matches Lock In renders sample for sample.
struct KickOscillatorState {
    double bodyPhase = 0.0;
    double clickPhase = 0.0;
    float dcX = 0.0f;
    float dcY = 0.0f;
    uint32_t noise = 0x6D2B79F5u;
    double snarePhase = 0.0;
    float snareLow = 0.0f;
    float snareHigh = 0.0f;
    uint32_t snareNoise = 0x51A4E001u;
    float clapLow = 0.0f;
    float clapHigh = 0.0f;
    uint32_t clapNoise = 0xC1A90001u;
    std::array<double, 6> hatPhase{};
    float hatLow = 0.0f;
    uint32_t hatNoise = 0x8A770001u;
};

// Synthesizes a complete single-shot kick waveform into a float buffer.
[[nodiscard]] std::vector<float> renderKickWaveform(
    const KickSynthSettings& settings,
    double sampleRate,
    float semitoneOffset = 0.0f) noexcept;

// Encodes mono float samples into a standard 32-bit IEEE float RIFF WAV byte buffer.
[[nodiscard]] std::vector<uint8_t> encodeWavBytes(const std::vector<float>& samples, uint32_t sampleRate);

// Decodes a 32-bit IEEE float RIFF WAV byte buffer back into mono float samples and sample rate.
[[nodiscard]] bool decodeWavBytes(
    const std::vector<uint8_t>& wavBytes,
    std::vector<float>& outSamples,
    uint32_t& outSampleRate);

// Manages the "Lock In" cached WAV state inside the plugin.
// When locked in, the plugin stops synthesizing the kick in real time and plays from the cached WAV.
class KickWavCache {
public:
    void prepare(double sampleRate);
    void lockIn(const KickSynthSettings& settings, double sampleRate);
    void unlock() noexcept;
    bool loadFromWavBytes(const std::vector<uint8_t>& wavBytes);

    [[nodiscard]] bool isLocked() const noexcept { return locked_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] const std::vector<float>& samples() const noexcept { return samples_; }
    [[nodiscard]] const std::vector<uint8_t>& wavBytes() const noexcept { return wavBytes_; }
    [[nodiscard]] float sampleAt(double index) const noexcept;

private:
    bool locked_ = false;
    double sampleRate_ = 48000.0;
    std::vector<float> samples_;
    std::vector<uint8_t> wavBytes_;
};

// Real-time kick voice that either synthesizes audio live or plays from the Locked-In WAV cache.
class KickVoice {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void triggerForward(
        const KickSynthSettings& settings,
        const KickWavCache& cache,
        double sampleRate,
        float velocity = 1.0f,
        float semitoneOffset = 0.0f,
        float effectiveTailMs = -1.0f) noexcept;

    void triggerReverse(
        const KickSynthSettings& settings,
        const KickWavCache& cache,
        double sampleRate,
        int durationSamples,
        float velocity = 1.0f,
        float semitoneOffset = 0.0f) noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] bool isReversing() const noexcept { return reversing_; }
    [[nodiscard]] bool isUsingCachedWav() const noexcept { return usingCache_; }

    void armMusicalReverse(double startPpq, double endPpq) noexcept;

    [[nodiscard]] float nextSample(
        const KickSynthSettings& liveSettings,
        const KickWavCache& cache,
        double sampleRate,
        double currentPpq = -1.0) noexcept;

private:
    [[nodiscard]] float synthesizeCurrentSample(const KickSynthSettings& settings, double sampleRate) noexcept;
    void updateDeclick(double sampleRate) noexcept;

    bool active_ = false;
    bool reversing_ = false;
    bool usingCache_ = false;
    int sampleIndex_ = 0;
    int bodySamples_ = 0;
    int totalSamples_ = 0;
    KickOscillatorState osc_{};
    double cacheReadPos_ = 0.0;
    double cacheReadStep_ = 1.0;
    float velocity_ = 1.0f;
    float semitoneOffset_ = 0.0f;
    float overrideTailMs_ = -1.0f;
    float lastOutput_ = 0.0f;
    float declick_ = 0.0f;
    float declickCoeff_ = 0.95f;
    bool reverseMusical_ = false;
    double reverseStartPpq_ = 0.0;
    double reverseEndPpq_ = 0.0;
    std::vector<float> reverseBuffer_;
};

} // namespace openedmkick::core
