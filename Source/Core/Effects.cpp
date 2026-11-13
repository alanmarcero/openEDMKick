#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace openedmkick::core {
namespace {

constexpr double kPpqEpsilon = 1.0e-9;
constexpr double kMinRate = 8000.0;
constexpr double kLimiterReleaseMs = 55.0;

[[nodiscard]] double nextBoundary(double ppq, double grid) noexcept {
    const double safeGrid = std::max(1.0e-4, grid);
    const double position = std::max(0.0, ppq);
    const double boundary = (std::floor(position / safeGrid + kPpqEpsilon) + 1.0) * safeGrid;
    return boundary <= position + kPpqEpsilon ? boundary + safeGrid : boundary;
}

[[nodiscard]] float finiteOr(float value, float fallback = 0.0f) noexcept {
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] float lowPassCoefficient(float cutoff, double sampleRate) noexcept {
    const double rate = std::max(kMinRate, sampleRate);
    const double hz = std::clamp(static_cast<double>(cutoff), 5.0, rate * 0.45);
    return static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * hz / rate));
}

float onePoleLow(float input, float& state, float coefficient) noexcept {
    state += coefficient * (input - state);
    if (!std::isfinite(state) || std::abs(state) > 16.0f) state = 0.0f;
    return state;
}

} // namespace

bool shouldTriggerReverse(ReverseSyncMode mode, bool noteReverseCue, double ppqPosition, double beatsPerBar) noexcept {
    if (noteReverseCue) return true;
    if (mode == ReverseSyncMode::Off) return false;
    if (mode != ReverseSyncMode::Beat4Auto) return true;
    const double bar = std::max(1.0, beatsPerBar);
    const double beat = std::fmod(std::max(0.0, ppqPosition), bar);
    return static_cast<int>(std::floor(beat + 1.0e-7)) == std::max(0, static_cast<int>(std::ceil(bar)) - 1);
}

double reverseTargetPpq(ReverseSyncMode mode, double ppqPosition, double beatsPerBar) noexcept {
    const double ppq = std::max(0.0, ppqPosition);
    switch (mode) {
        case ReverseSyncMode::HalfBeat: return nextBoundary(ppq, 0.5);
        case ReverseSyncMode::QuarterBeat: return nextBoundary(ppq, 0.25);
        case ReverseSyncMode::Beat4Auto: {
            const double bar = std::max(1.0, beatsPerBar);
            return (std::floor(ppq / bar + kPpqEpsilon) + 1.0) * bar;
        }
        case ReverseSyncMode::Off:
        case ReverseSyncMode::BeatEnd: return nextBoundary(ppq, 1.0);
    }
    return nextBoundary(ppq, 1.0);
}

int samplesUntilPpq(double ppqPosition, double targetPpq, double bpm, double sampleRate) noexcept {
    if (!std::isfinite(ppqPosition) || !std::isfinite(targetPpq) || targetPpq <= ppqPosition) return 0;
    const double ppqPerSample = std::clamp(bpm, 20.0, 400.0) / (60.0 * std::max(kMinRate, sampleRate));
    const double count = (targetPpq - ppqPosition) / ppqPerSample;
    return static_cast<int>(std::clamp(std::ceil(count - kPpqEpsilon), 0.0, 2147483647.0));
}

int computeSyncedReverseSamples(ReverseSyncMode mode, double ppqPosition, double bpm, double beatsPerBar, double sampleRate) noexcept {
    return samplesUntilPpq(ppqPosition, reverseTargetPpq(mode, ppqPosition, beatsPerBar), bpm, sampleRate);
}

float saturate(float input, float drive) noexcept {
    const float dry = finiteOr(input);
    const float amount = std::clamp(finiteOr(drive), 0.0f, 1.0f);
    if (amount < 1.0e-5f) return dry;
    const float gain = 1.0f + amount * 10.0f;
    const float wet = std::tanh(dry * gain) / std::tanh(gain);
    return dry + amount * (wet - dry);
}

void BitCrusher::reset() noexcept {
    phase_ = 1.0f;
    held_ = 0.0f;
}

float BitCrusher::process(float input, float rateHz, int bits, double sampleRate) noexcept {
    const double rate = std::max(kMinRate, sampleRate);
    const float hz = std::clamp(finiteOr(rateHz, 8000.0f), 100.0f, static_cast<float>(rate));
    const int depth = std::clamp(bits, 1, 16);
    phase_ += static_cast<float>(hz / rate);
    if (phase_ >= 1.0f) {
        phase_ -= std::floor(phase_);
        const float levels = std::exp2(static_cast<float>(depth - 1));
        held_ = std::round(std::clamp(finiteOr(input), -1.0f, 1.0f) * levels) / levels;
    }
    return held_;
}

void BeatStutter::prepare(double sampleRate) {
    const auto size = static_cast<size_t>(std::max(kMinRate, sampleRate) * 2.0) + 4u;
    history_.assign(size, 0.0f);
    reset();
}

void BeatStutter::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0f);
    writePos_ = 0;
    available_ = 0;
}

float BeatStutter::process(float input, double ppqPosition, double divisionBeats, double bpm, double sampleRate) noexcept {
    if (history_.empty()) return finiteOr(input);
    const int capacity = static_cast<int>(history_.size());
    const double rate = std::max(kMinRate, sampleRate);
    const double beatSamples = rate * 60.0 / std::clamp(bpm, 20.0, 400.0);
    const double division = std::clamp(divisionBeats, 0.0625, 1.0);
    const int delay = std::clamp(static_cast<int>(std::round(beatSamples * division)), 1, capacity - 1);
    history_[static_cast<size_t>(writePos_)] = finiteOr(input);
    const int read = (writePos_ + capacity - delay) % capacity;
    const float repeat = available_ >= delay ? history_[static_cast<size_t>(read)] : finiteOr(input);
    writePos_ = (writePos_ + 1) % capacity;
    available_ = std::min(available_ + 1, capacity - 1);

    const double cycle = std::max(0.0, ppqPosition) / division;
    const float phase = static_cast<float>(cycle - std::floor(cycle));
    const float edge = std::min(phase, 1.0f - phase);
    const float fade = std::clamp(edge * static_cast<float>(beatSamples * division) / 48.0f, 0.0f, 1.0f);
    return finiteOr(input) + fade * (repeat - finiteOr(input));
}

float TempoGate::gainAt(double ppqPosition, double divisionBeats, float depth) noexcept {
    const float amount = std::clamp(finiteOr(depth), 0.0f, 1.0f);
    if (amount <= 1.0e-5f) return 1.0f;
    const double division = std::max(0.0625, divisionBeats);
    const double cycle = std::max(0.0, ppqPosition) / division;
    const float phase = static_cast<float>(cycle - std::floor(cycle));
    const float gate = phase < 0.43f ? 1.0f : (phase > 0.57f ? 0.0f : (0.57f - phase) / 0.14f);
    const float shaped = gate * gate * (3.0f - 2.0f * gate);
    return 1.0f - amount * (1.0f - shaped);
}

void SyncFilterPump::reset() noexcept {
    ic1eq_ = 0.0f;
    ic2eq_ = 0.0f;
}

float SyncFilterPump::process(float input, double ppqPosition, double divisionBeats, float depth, double sampleRate) noexcept {
    if (!std::isfinite(input)) {
        reset();
        return 0.0f;
    }
    const float dry = finiteOr(input);
    const float amount = std::clamp(finiteOr(depth), 0.0f, 1.0f);
    if (amount < 1.0e-5f) { reset(); return dry; }

    const double division = std::max(0.0625, divisionBeats);
    const double cycle = std::max(0.0, ppqPosition) / division;
    const float phase = static_cast<float>(cycle - std::floor(cycle));
    const float motion = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * phase);
    const float cutoff = 70.0f + 9200.0f * motion * motion;
    const float g = std::tan(static_cast<float>(std::numbers::pi * cutoff / std::max(kMinRate, sampleRate)));
    constexpr float k = 0.82f;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = dry - ic2eq_;
    const float v1 = a1 * ic1eq_ + a2 * v3;
    const float v2 = ic2eq_ + a2 * ic1eq_ + a3 * v3;
    ic1eq_ = 2.0f * v1 - ic1eq_;
    ic2eq_ = 2.0f * v2 - ic2eq_;
    if (!std::isfinite(ic1eq_) || !std::isfinite(ic2eq_) || std::abs(ic1eq_) > 16.0f || std::abs(ic2eq_) > 16.0f) {
        reset();
        return dry;
    }
    const float wet = v2 + 0.25f * v1;
    return dry + amount * (wet - dry);
}

void ToneFilter::reset() noexcept {
    hpState1_ = hpState2_ = lpState1_ = lpState2_ = 0.0f;
}

float ToneFilter::highPass(float input, float cutoffHz, double sampleRate) noexcept {
    const auto a = lowPassCoefficient(cutoffHz, sampleRate);
    const float first = input - onePoleLow(input, hpState1_, a);
    return first - onePoleLow(first, hpState2_, a);
}

float ToneFilter::lowPass(float input, float cutoffHz, double sampleRate) noexcept {
    const auto a = lowPassCoefficient(cutoffHz, sampleRate);
    return onePoleLow(onePoleLow(input, lpState1_, a), lpState2_, a);
}

float ToneFilter::process(float input, float highPassHz, float lowPassHz, double sampleRate) noexcept {
    const float dry = finiteOr(input);
    float shaped = dry;
    if (std::isfinite(highPassHz) && highPassHz > kHighPassBypassHz + 0.5f) shaped = highPass(dry, highPassHz, sampleRate);
    else hpState1_ = hpState2_ = 0.0f;
    if (std::isfinite(lowPassHz) && lowPassHz < kLowPassBypassHz - 1.0f) return lowPass(shaped, lowPassHz, sampleRate);
    lpState1_ = lpState2_ = shaped;
    return shaped;
}

void PeakLimiter::prepare(double sampleRate) noexcept {
    releaseCoefficient_ = static_cast<float>(std::exp(-1.0 / (kLimiterReleaseMs * 0.001 * std::max(kMinRate, sampleRate))));
    reset();
}

void PeakLimiter::reset() noexcept { gain_ = 1.0f; }

void PeakLimiter::process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept {
    const float limit = std::clamp(finiteOr(ceiling, 1.0f), 0.05f, 1.0f);
    const int count = std::clamp(numChannels, 0, kMaxChannels);
    for (int sample = 0; sample < numSamples; ++sample) {
        float peak = 0.0f;
        for (int channel = 0; channel < count; ++channel) {
            auto& value = channels[channel][sample];
            value = finiteOr(value);
            peak = std::max(peak, std::abs(value));
        }
        const float target = peak > limit ? limit / peak : 1.0f;
        gain_ = target < gain_ ? target : target + releaseCoefficient_ * (gain_ - target);
        for (int channel = 0; channel < count; ++channel)
            channels[channel][sample] = std::clamp(channels[channel][sample] * gain_, -limit, limit);
    }
}

} // namespace openedmkick::core
