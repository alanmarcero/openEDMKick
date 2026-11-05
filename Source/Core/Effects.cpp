#include "Effects.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openedmkick::core {

namespace {

constexpr float kMaxDriveGain = 12.0f;
constexpr double kLimiterReleaseMs = 65.0;
constexpr double kBoundaryEpsilon = 1e-4;

[[nodiscard]] double remainingBeatsToGrid(double ppqPosition, double gridBeats) noexcept {
    const double safeGrid = std::max(0.0625, gridBeats);
    const double positivePpq = std::max(0.0, ppqPosition);
    const double cycles = positivePpq / safeGrid;
    const double frac = cycles - std::floor(cycles + kBoundaryEpsilon);
    const double remaining = (1.0 - frac) * safeGrid;
    return remaining <= safeGrid * kBoundaryEpsilon ? safeGrid : remaining;
}

} // namespace

bool shouldTriggerReverse(
    ReverseSyncMode mode,
    bool noteReverseCue,
    double ppqPosition,
    double beatsPerBar) noexcept {
    if (noteReverseCue) {
        return true;
    }
    switch (mode) {
        case ReverseSyncMode::Off:
            return false;
        case ReverseSyncMode::BeatEnd:
        case ReverseSyncMode::HalfBeat:
        case ReverseSyncMode::QuarterBeat:
            return true;
        case ReverseSyncMode::Beat4Auto: {
            const double safeBeatsPerBar = std::max(1.0, beatsPerBar);
            const double barPos = std::fmod(std::max(0.0, ppqPosition), safeBeatsPerBar);
            const int beatIndex = static_cast<int>(std::floor(barPos + kBoundaryEpsilon));
            const int lastBeatIndex = std::max(0, static_cast<int>(std::round(safeBeatsPerBar)) - 1);
            return beatIndex == lastBeatIndex;
        }
    }
    return false;
}

int computeSyncedReverseSamples(
    ReverseSyncMode mode,
    double ppqPosition,
    double bpm,
    double beatsPerBar,
    double sampleRate) noexcept {
    const double safeBpm = std::max(20.0, bpm);
    const double safeRate = std::max(8000.0, sampleRate);
    const double samplesPerBeat = (60.0 / safeBpm) * safeRate;
    const double safeBeatsPerBar = std::max(1.0, beatsPerBar);

    const double gridBeats = [&] {
        switch (mode) {
            case ReverseSyncMode::HalfBeat: return 0.5;
            case ReverseSyncMode::QuarterBeat: return 0.25;
            case ReverseSyncMode::Beat4Auto: {
                const double barPos = std::fmod(std::max(0.0, ppqPosition), safeBeatsPerBar);
                const double toBarEnd = safeBeatsPerBar - barPos;
                return (toBarEnd > kBoundaryEpsilon && toBarEnd <= 1.0 + kBoundaryEpsilon) ? 1.0 : 1.0;
            }
            case ReverseSyncMode::Off:
            case ReverseSyncMode::BeatEnd:
                break;
        }
        return 1.0;
    }();

    const double remainingBeats = remainingBeatsToGrid(ppqPosition, gridBeats);
    return std::max(16, static_cast<int>(std::round(remainingBeats * samplesPerBeat)));
}

float saturate(float input, float drive) noexcept {
    const float clampedDrive = std::clamp(drive, 0.0f, 1.0f);
    if (clampedDrive <= 0.0001f) {
        return input;
    }
    const float gain = 1.0f + kMaxDriveGain * clampedDrive;
    const float saturated = std::tanh(gain * input) / std::sqrt(gain);
    return input + clampedDrive * (saturated - input);
}

void BitCrusher::reset() noexcept {
    phase_ = 0.0f;
    held_ = 0.0f;
}

float BitCrusher::process(float input, float rateHz, int bits, double sampleRate) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const float clampedRate = std::clamp(rateHz, 100.0f, static_cast<float>(safeRate));
    const int clampedBits = std::clamp(bits, 1, 16);
    const float advanced = phase_ + static_cast<float>(clampedRate / safeRate);
    phase_ = advanced - std::floor(advanced);
    if (advanced >= 1.0f) {
        const float steps = std::exp2(static_cast<float>(clampedBits));
        held_ = std::round(input * steps) / steps;
    }
    return held_;
}

void BeatStutter::prepare(double sampleRate) {
    const size_t capacity = static_cast<size_t>(std::max(8000.0, sampleRate) * 2.5) + 1u;
    history_.assign(capacity, 0.0f);
    reset();
}

void BeatStutter::reset() noexcept {
    writePos_ = 0;
    available_ = 0;
}

float BeatStutter::process(
    float input,
    double ppqPosition,
    double divisionBeats,
    double bpm,
    double sampleRate) noexcept {
    if (history_.empty()) {
        return input;
    }
    const int capacity = static_cast<int>(history_.size());
    const double safeBpm = std::max(20.0, bpm);
    const double safeRate = std::max(8000.0, sampleRate);
    const double beatSamples = safeRate * 60.0 / safeBpm;
    const double sliceBeats = std::clamp(divisionBeats, 0.0625, 1.0);
    const double sliceSamples = std::clamp(beatSamples * sliceBeats, 16.0, static_cast<double>(capacity - 1) / 4.0);

    const double beatPhase = ppqPosition - std::floor(ppqPosition);
    const int sliceIndex = static_cast<int>(std::floor(beatPhase / sliceBeats)) % 4;
    const int delay = std::clamp(static_cast<int>(std::round(static_cast<double>(sliceIndex) * sliceSamples)), 0, capacity - 1);

    history_[static_cast<size_t>(writePos_)] = input;
    float output = input;
    if (delay > 0 && available_ >= delay) {
        const int readPos = (writePos_ + capacity - delay) % capacity;
        const float repeated = history_[static_cast<size_t>(readPos)];
        const double slicePos = (beatPhase / sliceBeats) - std::floor(beatPhase / sliceBeats);
        const double edge = std::min(slicePos, 1.0 - slicePos);
        const float fade = static_cast<float>(std::min(1.0, (edge * sliceSamples) / (0.002 * safeRate)));
        output = input + fade * (repeated - input);
    }

    writePos_ = (writePos_ + 1) % capacity;
    available_ = std::min(available_ + 1, capacity - 1);
    return output;
}

float TempoGate::gainAt(double ppqPosition, double divisionBeats, float depth) noexcept {
    const float clampedDepth = std::clamp(depth, 0.0f, 1.0f);
    if (clampedDepth <= 0.0001f) {
        return 1.0f;
    }
    const double safeDiv = std::max(0.0625, divisionBeats);
    const double cycles = std::max(0.0, ppqPosition) / safeDiv;
    const float phase = static_cast<float>(cycles - std::floor(cycles));
    const float smoothWindow = 0.06f;
    const float attack = std::min(1.0f, phase / smoothWindow);
    const float gateOff = phase < 0.5f ? 1.0f : std::max(0.0f, 1.0f - (phase - 0.5f) / smoothWindow);
    const float gateShape = attack * gateOff;
    return 1.0f - clampedDepth * (1.0f - gateShape);
}

void SyncFilterPump::reset() noexcept {
    ic1eq_ = 0.0f;
    ic2eq_ = 0.0f;
}

float SyncFilterPump::process(
    float input,
    double ppqPosition,
    double divisionBeats,
    float depth,
    double sampleRate) noexcept {
    const float clampedDepth = std::clamp(depth, 0.0f, 1.0f);
    if (clampedDepth <= 0.0001f) {
        return input;
    }
    const double safeDiv = std::max(0.0625, divisionBeats);
    const double cycles = std::max(0.0, ppqPosition) / safeDiv;
    const float phase = static_cast<float>(cycles - std::floor(cycles));
    const float sweep = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * phase));
    const float cutoffHz = 90.0f * std::pow(90.0f, sweep);
    const double safeRate = std::max(8000.0, sampleRate);
    const float g = static_cast<float>(std::tan(std::numbers::pi * std::clamp(static_cast<double>(cutoffHz), 40.0, safeRate * 0.42) / safeRate));
    const float k = 1.1f;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = input - ic2eq_;
    const float v1 = a1 * ic1eq_ + a2 * v3;
    const float v2 = ic2eq_ + a2 * ic1eq_ + a3 * v3;
    ic1eq_ = 2.0f * v1 - ic1eq_;
    ic2eq_ = 2.0f * v2 - ic2eq_;
    const float filtered = v2 + 0.35f * v1;
    return input + clampedDepth * (filtered - input);
}

void PeakLimiter::prepare(double sampleRate) noexcept {
    releaseCoefficient_ = static_cast<float>(std::exp(-1.0 / (kLimiterReleaseMs * 0.001 * std::max(8000.0, sampleRate))));
    reset();
}

void PeakLimiter::reset() noexcept {
    releasedGain_ = 1.0f;
    position_ = 0;
    requiredGains_.fill(1.0f);
    releasedGains_.fill(1.0f);
    delayLines_ = {};
}

void PeakLimiter::process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept {
    const float safeCeiling = std::clamp(ceiling, 0.05f, 1.0f);
    const auto channelIndices = std::views::iota(0, std::min(numChannels, kMaxChannels));
    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        const float peak = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
            [&](float loudest, int ch) { return std::max(loudest, std::abs(channels[ch][sample])); });
        requiredGains_[static_cast<size_t>(position_)] = peak > safeCeiling ? safeCeiling / peak : 1.0f;

        const float heldGain = std::ranges::min(requiredGains_);
        releasedGain_ = std::min(heldGain, heldGain + releaseCoefficient_ * (releasedGain_ - heldGain));
        releasedGains_[static_cast<size_t>(position_)] = releasedGain_;
        const float gain = std::accumulate(releasedGains_.begin(), releasedGains_.end(), 0.0f)
            / static_cast<float>(kLookahead);

        const int oldest = (position_ + 1) % kLookahead;
        std::ranges::for_each(channelIndices, [&](int ch) {
            auto& delayLine = delayLines_[static_cast<size_t>(ch)];
            delayLine[static_cast<size_t>(position_)] = channels[ch][sample];
            channels[ch][sample] = std::clamp(delayLine[static_cast<size_t>(oldest)] * gain, -safeCeiling, safeCeiling);
        });
        position_ = oldest;
    });
}

} // namespace openedmkick::core
