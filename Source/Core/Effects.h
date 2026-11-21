#pragma once

#include "KickDsp.h"

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

class BitCrusher {
public:
    void reset() noexcept;
    [[nodiscard]] float process(float input, float rateHz, int bits, double sampleRate) noexcept;

private:
    double phase_ = 1.0;
    float held_ = 0.0f;
    float staged_ = 0.0f;
    bool stagedValid_ = false;
    double lastCutoffHz_ = 0.0;
    double lastRate_ = 0.0;
    dsp::Svf preFilter1_;
    dsp::Svf preFilter2_;
    dsp::Noise dither_;
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
    // A sweep goes to these limits while its cutoff is at the bypass end.
    static constexpr float kHighPassSweepTopHz = 8000.0f;
    static constexpr float kLowPassSweepFloorHz = 200.0f;

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

struct ThreeBandEqSettings {
    float lowFreqHz = 80.0f;
    float lowGainDb = 0.0f;
    float midFreqHz = 1000.0f;
    float midGainDb = 0.0f;
    float highFreqHz = 7500.0f;
    float highGainDb = 0.0f;

    bool operator==(const ThreeBandEqSettings&) const noexcept = default;
};

// Stereo 3-band EQ with log-frequency glide and +/-12 dB cut/boost per band.
class ThreeBandEq {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float& left, float& right, const ThreeBandEqSettings& settings) noexcept;

private:
    enum class BandType { lowShelf, peak, highShelf };
    struct Band {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1L = 0.0, z2L = 0.0, z1R = 0.0, z2R = 0.0;
        float smoothedLogFreq = 0.0f;
        float smoothedDb = 0.0f;
        bool initialized = false;
    };

    void updateBand(Band& band, BandType type, float minHz, float maxHz, float targetHz, float targetDb) noexcept;
    static void filterBand(Band& band, float& left, float& right) noexcept;

    double sampleRate_ = 48000.0;
    float glideCoeff_ = 0.001f;
    std::array<Band, 3> bands_{};
};

class PeakLimiter {
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLookahead = 64;
    static constexpr int kLatency = 64;
    static constexpr int kIspTaps = 12;
    static constexpr int kIspPhases = 4;
    static constexpr int kIspCenter = 5;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept;

private:
    std::array<std::array<float, kIspTaps>, kIspPhases> ispWeights_{};
    std::array<std::array<float, kIspTaps>, kMaxChannels> ispHistory_{};
    std::array<std::array<float, kLookahead>, kMaxChannels> delayBuffer_{};
    std::array<float, kLookahead> peakBuffer_{};
    size_t writeIndex_ = 0;
    float fastReleaseCoeff_ = 0.0f;
    float slowReleaseCoeff_ = 0.0f;
    float peakEnvCoeff_ = 0.0f;
    float rmsEnvCoeff_ = 0.0f;
    float peakEnv_ = 0.0f;
    float rmsEnv_ = 0.0f;
    float gain_ = 1.0f;
};

} // namespace openedmkick::core
