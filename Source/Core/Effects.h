#pragma once

#include <array>
#include <vector>

namespace openedmkick::core {

enum class ReverseSyncMode {
    Off = 0,
    BeatEnd = 1,     // Always finishes at the exact end of the current beat
    Beat4Auto = 2,   // Automatically reverses on Beat 4 (last beat of bar), finishing at the end of Beat 4
    HalfBeat = 3,    // Finishes on the next 1/2-beat boundary
    QuarterBeat = 4  // Finishes on the next 1/4-beat boundary
};

// Sync subdivisions in beats: 1/4 (1.0), 1/8 (0.5), 1/16 (0.25), 1/32 (0.125), 1/64 (0.0625).
inline constexpr std::array<double, 5> kSyncDivisionBeats = {
    1.0, 0.5, 0.25, 0.125, 0.0625
};

// Returns true if a kick triggered at ppqPosition should play in reverse under the active ReverseSyncMode.
[[nodiscard]] bool shouldTriggerReverse(
    ReverseSyncMode mode,
    bool noteReverseCue,
    double ppqPosition,
    double beatsPerBar) noexcept;

// Computes the exact number of samples until the target beat/subdivision boundary so the reverse kick
// finishes in exact lockstep with the host clock (e.g., finishing right at the end of Beat 4).
// Musical PPQ where a reverse swell must stop. Beat 4 Auto uses the bar end.
[[nodiscard]] double reverseTargetPpq(
    ReverseSyncMode mode,
    double ppqPosition,
    double beatsPerBar) noexcept;

// Samples to play so the last reverse sample is the one before targetPpq.
[[nodiscard]] int samplesUntilPpq(
    double ppqPosition,
    double targetPpq,
    double bpm,
    double sampleRate) noexcept;

[[nodiscard]] int computeSyncedReverseSamples(
    ReverseSyncMode mode,
    double ppqPosition,
    double bpm,
    double beatsPerBar,
    double sampleRate) noexcept;

// tanh drive saturation blended by drive (0..1).
[[nodiscard]] float saturate(float input, float drive) noexcept;

// Sample-rate reducer and bit crusher.
class BitCrusher {
public:
    void reset() noexcept;
    [[nodiscard]] float process(float input, float rateHz, int bits, double sampleRate) noexcept;

private:
    float phase_ = 0.0f;
    float held_ = 0.0f;
};

// Host-clock-synced beat repeat / stutter effect.
class BeatStutter {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    [[nodiscard]] float process(
        float input,
        double ppqPosition,
        double divisionBeats,
        double bpm,
        double sampleRate) noexcept;

private:
    std::vector<float> history_;
    int writePos_ = 0;
    int available_ = 0;
};

// Host-clock-synced rhythmic gate chopper.
class TempoGate {
public:
    [[nodiscard]] static float gainAt(
        double ppqPosition,
        double divisionBeats,
        float depth) noexcept;
};

// Host-clock-synced resonant low-pass / band-emphasis pump filter.
class SyncFilterPump {
public:
    void reset() noexcept;
    [[nodiscard]] float process(
        float input,
        double ppqPosition,
        double divisionBeats,
        float depth,
        double sampleRate) noexcept;

private:
    float ic1eq_ = 0.0f;
    float ic2eq_ = 0.0f;
};

// Fixed 12 dB/oct high-pass then low-pass. No resonance control.
// The minimum high-pass and maximum low-pass return the input unchanged.
class ToneFilter {
public:
    static constexpr float kHighPassBypassHz = 20.0f;
    static constexpr float kLowPassBypassHz = 20000.0f;

    void reset() noexcept;
    [[nodiscard]] float process(float input, float highPassHz, float lowPassHz, double sampleRate) noexcept;

private:
    [[nodiscard]] float highPass(float input, float cutoffHz, double sampleRate) noexcept;
    [[nodiscard]] float lowPass(float input, float cutoffHz, double sampleRate) noexcept;

    float hpState1_ = 0.0f;
    float hpState2_ = 0.0f;
    float lpState1_ = 0.0f;
    float lpState2_ = 0.0f;
};

// Zero-latency peak limiter. Instant attack keeps the kick transient on the
// MIDI/grid sample; release recovers gain without a lookahead delay.
class PeakLimiter {
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLookahead = 0;
    static constexpr int kLatency = 0;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept;

private:
    float releaseCoefficient_ = 0.0f;
    float gain_ = 1.0f;
};

} // namespace openedmkick::core
