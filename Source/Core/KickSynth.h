#pragma once

#include <array>
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

    bool operator==(const KickSynthSettings&) const noexcept = default;
};

// Returns the tuned fundamental frequency in Hz for any of the 12 chromatic notes.
[[nodiscard]] float fundamentalHzForNote(int noteIndex, int octave = 1, float fineCents = 0.0f) noexcept;

// Shapes a normalized progress (0..1) with a bipolar curve (-1..+1).
[[nodiscard]] float shapeCurve(float progress, float curve) noexcept;

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

    [[nodiscard]] float nextSample(
        const KickSynthSettings& liveSettings,
        const KickWavCache& cache,
        double sampleRate) noexcept;

private:
    [[nodiscard]] float synthesizeCurrentSample(const KickSynthSettings& settings, double sampleRate) noexcept;

    bool active_ = false;
    bool reversing_ = false;
    bool usingCache_ = false;
    int sampleIndex_ = 0;
    int totalSamples_ = 0;
    double bodyPhase_ = 0.0;
    double clickPhase_ = 0.0;
    double cacheReadPos_ = 0.0;
    double cacheReadStep_ = 1.0;
    float velocity_ = 1.0f;
    float semitoneOffset_ = 0.0f;
    float overrideTailMs_ = -1.0f;
    std::vector<float> reverseBuffer_;
};

} // namespace openedmkick::core
