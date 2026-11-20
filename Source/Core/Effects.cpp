#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace openedmkick::core {
namespace {

constexpr double kPpqEpsilon = 1.0e-9;
constexpr double kMinRate = 8000.0;

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

void BitCrusher::reset() noexcept {
    phase_ = 1.0;
    held_ = 0.0f;
    staged_ = 0.0f;
    stagedValid_ = false;
    lastCutoffHz_ = 0.0;
    lastRate_ = 0.0;
    preFilter1_.reset();
    preFilter2_.reset();
    dither_.seed(0x9E3779B9u);
}

float BitCrusher::process(float input, float rateHz, int bits, double sampleRate) noexcept {
    if (!std::isfinite(input)) {
        reset();
        return 0.0f;
    }
    const double rate = std::max(kMinRate, sampleRate);
    const float dry = std::clamp(input, -1.0f, 1.0f);
    const float hz = std::clamp(finiteOr(rateHz, 8000.0f), 100.0f, static_cast<float>(rate));
    const int depth = std::clamp(bits, 1, 16);
    const float levels = std::exp2(static_cast<float>(depth - 1));
    const double dt = static_cast<double>(hz) / rate;
    const bool decimating = dt < 0.999;

    float filtered = dry;
    if (decimating) {
        const double cutoffHz = std::clamp(0.45 * static_cast<double>(hz), 40.0, rate * 0.48);
        if (std::abs(cutoffHz - lastCutoffHz_) > 1.0e-3 || std::abs(rate - lastRate_) > 1.0e-3) {
            lastCutoffHz_ = cutoffHz;
            lastRate_ = rate;
            preFilter1_.c = dsp::lowpassCoeffs(cutoffHz, 0.54119610, rate);
            preFilter2_.c = dsp::lowpassCoeffs(cutoffHz, 1.30656296, rate);
        }
        filtered = preFilter2_.process(preFilter1_.process(dry));
    }
    if (!decimating) {
        preFilter1_.reset();
        preFilter2_.reset();
        lastCutoffHz_ = 0.0;
    }

    const auto quantize = [&](float x) noexcept {
        float tpdf = 0.0f;
        if (depth < 16 && std::abs(x) > 1.0e-6f) {
            tpdf = 0.49f * (dither_.next() - dither_.next()) * 0.5f;
        }
        return std::clamp(std::round(x * levels + tpdf) / levels, -1.0f, 1.0f);
    };

    if (!decimating) {
        stagedValid_ = false;
        held_ = quantize(filtered);
        return held_;
    }

    float currentSample = held_;
    phase_ += dt;
    float preEdgeCorrection = 0.0f;
    if (phase_ >= 1.0) {
        phase_ -= std::floor(phase_);
        const float frac = static_cast<float>(std::clamp(phase_ / dt, 0.0, 1.0));
        const float previousHeld = held_;
        held_ = quantize(filtered);
        const float step = held_ - previousHeld;
        const float invFrac = 1.0f - frac;
        preEdgeCorrection = 0.5f * frac * frac * step;
        currentSample = held_ - 0.5f * invFrac * invFrac * step;
    }
    if (!stagedValid_) {
        staged_ = currentSample;
        stagedValid_ = true;
        return currentSample;
    }
    const float out = staged_ + preEdgeCorrection;
    staged_ = currentSample;
    return std::clamp(out, -1.0f, 1.0f);
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

void ThreeBandEq::prepare(double sampleRate) noexcept {
    sampleRate_ = std::max(kMinRate, std::isfinite(sampleRate) ? sampleRate : 48000.0);
    glideCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.020 * sampleRate_)));
    reset();
}

void ThreeBandEq::reset() noexcept {
    std::ranges::for_each(bands_, [](Band& band) {
        band = Band{};
    });
}

void ThreeBandEq::updateBand(Band& band, BandType type, float minHz, float maxHz, float targetHz, float targetDb) noexcept {
    const float nyquistLimit = static_cast<float>(sampleRate_ * 0.45);
    const float clampedHz = std::clamp(finiteOr(targetHz, minHz), minHz, std::min(maxHz, nyquistLimit));
    const float clampedDb = std::clamp(finiteOr(targetDb, 0.0f), -12.0f, 12.0f);
    const float targetLog = std::log(std::max(10.0f, clampedHz));
    const bool firstSample = !band.initialized;
    const bool changing = std::abs(band.smoothedLogFreq - targetLog) > 1.0e-5f
        || std::abs(band.smoothedDb - clampedDb) > 1.0e-4f;

    if (!firstSample && !changing) return;

    band.smoothedLogFreq = firstSample ? targetLog : band.smoothedLogFreq + glideCoeff_ * (targetLog - band.smoothedLogFreq);
    band.smoothedDb = firstSample ? clampedDb : band.smoothedDb + glideCoeff_ * (clampedDb - band.smoothedDb);
    band.initialized = true;

    if (std::abs(band.smoothedDb) < 0.002f) {
        band.b0 = 1.0;
        band.b1 = band.b2 = band.a1 = band.a2 = 0.0;
        return;
    }

    const double hz = std::clamp(static_cast<double>(std::exp(band.smoothedLogFreq)), 10.0, sampleRate_ * 0.45);
    const double w0 = 2.0 * std::numbers::pi * hz / sampleRate_;
    const double c = std::cos(w0);
    const double s = std::sin(w0);
    const double a = std::pow(10.0, static_cast<double>(band.smoothedDb) / 40.0);

    struct Coeffs { double b0, b1, b2, a0, a1, a2; };
    const auto design = [&](BandType kind) noexcept -> Coeffs {
        if (kind == BandType::peak) {
            const double alpha = s / (2.0 * 0.9);
            return { 1.0 + alpha * a, -2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * c, 1.0 - alpha / a };
        }
        const double alpha = s / std::numbers::sqrt2;
        const double t = 2.0 * std::sqrt(a) * alpha;
        if (kind == BandType::lowShelf) {
            return {
                a * ((a + 1.0) - (a - 1.0) * c + t),
                2.0 * a * ((a - 1.0) - (a + 1.0) * c),
                a * ((a + 1.0) - (a - 1.0) * c - t),
                (a + 1.0) + (a - 1.0) * c + t,
                -2.0 * ((a - 1.0) + (a + 1.0) * c),
                (a + 1.0) + (a - 1.0) * c - t
            };
        }
        return {
            a * ((a + 1.0) + (a - 1.0) * c + t),
            -2.0 * a * ((a - 1.0) + (a + 1.0) * c),
            a * ((a + 1.0) + (a - 1.0) * c - t),
            (a + 1.0) - (a - 1.0) * c + t,
            2.0 * ((a - 1.0) - (a + 1.0) * c),
            (a + 1.0) - (a - 1.0) * c - t
        };
    };
    const auto coeffs = design(type);
    const double invA0 = 1.0 / coeffs.a0;
    band.b0 = coeffs.b0 * invA0;
    band.b1 = coeffs.b1 * invA0;
    band.b2 = coeffs.b2 * invA0;
    band.a1 = coeffs.a1 * invA0;
    band.a2 = coeffs.a2 * invA0;
}

void ThreeBandEq::filterBand(Band& band, float& left, float& right) noexcept {
    if (std::abs(band.smoothedDb) < 0.002f) {
        band.z1L = band.z2L = band.z1R = band.z2R = 0.0;
        return;
    }
    const auto tick = [&](float in, double& z1, double& z2) noexcept {
        const double x = static_cast<double>(finiteOr(in));
        const double y = band.b0 * x + z1;
        z1 = band.b1 * x - band.a1 * y + z2;
        z2 = band.b2 * x - band.a2 * y;
        if (!std::isfinite(y) || !std::isfinite(z1) || !std::isfinite(z2)) {
            z1 = z2 = 0.0;
            return 0.0f;
        }
        return static_cast<float>(y);
    };
    left = tick(left, band.z1L, band.z2L);
    right = tick(right, band.z1R, band.z2R);
}

void ThreeBandEq::process(float& left, float& right, const ThreeBandEqSettings& settings) noexcept {
    updateBand(bands_[0], BandType::lowShelf, 25.0f, 400.0f, settings.lowFreqHz, settings.lowGainDb);
    updateBand(bands_[1], BandType::peak, 200.0f, 6000.0f, settings.midFreqHz, settings.midGainDb);
    updateBand(bands_[2], BandType::highShelf, 2000.0f, 18000.0f, settings.highFreqHz, settings.highGainDb);
    filterBand(bands_[0], left, right);
    filterBand(bands_[1], left, right);
    filterBand(bands_[2], left, right);
}

void PeakLimiter::prepare(double sampleRate) noexcept {
    const double rate = std::max(kMinRate, std::isfinite(sampleRate) ? sampleRate : 48000.0);
    fastReleaseCoeff_ = static_cast<float>(std::exp(-1.0 / (0.008 * rate)));
    slowReleaseCoeff_ = static_cast<float>(std::exp(-1.0 / (0.095 * rate)));
    peakEnvCoeff_ = static_cast<float>(std::exp(-1.0 / (0.025 * rate)));
    rmsEnvCoeff_ = static_cast<float>(std::exp(-1.0 / (0.040 * rate)));

    const auto besselI0 = [](double x) noexcept {
        double sum = 1.0;
        double term = 1.0;
        int k = 1;
        while (k <= 18) {
            const double halfRatio = (0.5 * x) / static_cast<double>(k);
            term *= halfRatio * halfRatio;
            sum += term;
            ++k;
        }
        return sum;
    };
    constexpr double beta = 5.5;
    const double invDenom = 1.0 / besselI0(beta);
    constexpr double halfSpan = 0.5 * static_cast<double>(kIspTaps);
    std::ranges::for_each(std::views::iota(0, kIspPhases), [&](int phaseIndex) {
        const double subPhase = static_cast<double>(phaseIndex) / static_cast<double>(kIspPhases);
        double weightSum = 0.0;
        std::ranges::for_each(std::views::iota(0, kIspTaps), [&](int tap) {
            const double delta = static_cast<double>(kIspCenter - tap) + subPhase;
            double sinc = 1.0;
            if (std::abs(delta) > 1.0e-9) {
                const double angle = std::numbers::pi * delta;
                sinc = std::sin(angle) / angle;
            }
            const double norm = std::clamp(delta / halfSpan, -1.0, 1.0);
            const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - norm * norm))) * invDenom;
            const double w = sinc * window;
            ispWeights_[static_cast<size_t>(phaseIndex)][static_cast<size_t>(tap)] = static_cast<float>(w);
            weightSum += w;
        });
        const float invSum = weightSum > 1.0e-9 ? static_cast<float>(1.0 / weightSum) : 1.0f;
        std::ranges::for_each(ispWeights_[static_cast<size_t>(phaseIndex)], [&](float& w) { w *= invSum; });
    });
    reset();
}

void PeakLimiter::reset() noexcept {
    std::ranges::for_each(ispHistory_, [](auto& hist) { hist.fill(0.0f); });
    std::ranges::for_each(delayBuffer_, [](auto& buf) { buf.fill(0.0f); });
    peakBuffer_.fill(0.0f);
    writeIndex_ = 0;
    peakEnv_ = 0.0f;
    rmsEnv_ = 0.0f;
    gain_ = 1.0f;
}

void PeakLimiter::process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept {
    const float limit = std::clamp(finiteOr(ceiling, 1.0f), 0.05f, 1.0f);
    const int count = std::clamp(numChannels, 0, kMaxChannels);
    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        float incomingPeak = 0.0f;
        float centerTruePeak = 0.0f;
        std::array<float, kMaxChannels> exiting{};

        std::ranges::for_each(std::views::iota(0, count), [&](int ch) {
            const auto chIdx = static_cast<size_t>(ch);
            const float cleanIn = finiteOr(channels[ch][sample]);
            exiting[chIdx] = delayBuffer_[chIdx][writeIndex_];
            delayBuffer_[chIdx][writeIndex_] = cleanIn;
            incomingPeak = std::max(incomingPeak, std::abs(cleanIn));

            auto& hist = ispHistory_[chIdx];
            int shift = kIspTaps - 1;
            while (shift > 0) {
                hist[static_cast<size_t>(shift)] = hist[static_cast<size_t>(shift - 1)];
                --shift;
            }
            hist[0] = cleanIn;

            std::ranges::for_each(std::views::iota(0, kIspPhases), [&](int p) {
                const auto& weights = ispWeights_[static_cast<size_t>(p)];
                float acc = 0.0f;
                std::ranges::for_each(std::views::iota(0, kIspTaps), [&](int tap) {
                    acc += weights[static_cast<size_t>(tap)] * hist[static_cast<size_t>(tap)];
                });
                centerTruePeak = std::max(centerTruePeak, std::abs(acc));
            });
        });

        const float exitingPeak = peakBuffer_[writeIndex_];
        peakBuffer_[writeIndex_] = incomingPeak;
        const size_t ispSlot = (writeIndex_ + static_cast<size_t>(kLookahead - kIspCenter)) % static_cast<size_t>(kLookahead);
        peakBuffer_[ispSlot] = std::max(peakBuffer_[ispSlot], centerTruePeak);
        writeIndex_ = (writeIndex_ + 1u) % static_cast<size_t>(kLookahead);

        const float power = exitingPeak * exitingPeak;
        peakEnv_ = std::max(power, peakEnv_ * peakEnvCoeff_);
        rmsEnv_ = power + rmsEnvCoeff_ * (rmsEnv_ - power);

        float immediateGain = 1.0f;
        if (exitingPeak > limit) immediateGain = limit / exitingPeak;
        if (gain_ > immediateGain) gain_ = immediateGain;

        float minSlope = 0.0f;
        std::ranges::for_each(std::views::iota(1, kLookahead + 1), [&](int dist) {
            const size_t idx = (writeIndex_ + static_cast<size_t>(dist - 1)) % static_cast<size_t>(kLookahead);
            const float futurePeak = peakBuffer_[idx];
            if (futurePeak <= limit) return;
            const float reqGain = limit / futurePeak;
            if (reqGain >= gain_) return;
            const float slope = (reqGain - gain_) / static_cast<float>(dist);
            minSlope = std::min(minSlope, slope);
        });

        if (minSlope < 0.0f) gain_ += minSlope;
        if (minSlope >= 0.0f) {
            const float crest = std::sqrt(peakEnv_ / std::max(rmsEnv_, 1.0e-7f));
            const float transientBlend = std::clamp((crest - 1.8f) / 3.2f, 0.0f, 1.0f);
            const float relCoeff = std::lerp(slowReleaseCoeff_, fastReleaseCoeff_, transientBlend);
            gain_ = immediateGain + relCoeff * (gain_ - immediateGain);
        }
        gain_ = std::clamp(gain_, 0.0f, 1.0f);

        std::ranges::for_each(std::views::iota(0, count), [&](int ch) {
            const float limited = exiting[static_cast<size_t>(ch)] * gain_;
            channels[ch][sample] = std::clamp(limited, -limit, limit);
        });
    });
}

} // namespace openedmkick::core
