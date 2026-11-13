#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

// Small, allocation-free DSP building blocks shared by the kick voice and analysis code.
namespace openedmkick::dsp {

inline constexpr double kTwoPi = 2.0 * std::numbers::pi;

[[nodiscard]] inline float dbToGain(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

[[nodiscard]] inline float finiteOr(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

struct BiquadCoeffs {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

namespace detail {

struct BiquadPrototype {
    double cosW = 1.0;
    double alpha = 0.0;
};

[[nodiscard]] inline BiquadPrototype prototype(double hz, double q, double rate) noexcept {
    const double safeRate = std::max(1000.0, rate);
    const double limited = std::clamp(hz, 5.0, safeRate * 0.49);
    const double w = kTwoPi * limited / safeRate;
    return { std::cos(w), std::sin(w) / (2.0 * std::max(0.05, q)) };
}

[[nodiscard]] inline BiquadCoeffs normalised(double b0, double b1, double b2, double a0, double a1, double a2) noexcept {
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

} // namespace detail

// RBJ cookbook designs.
[[nodiscard]] inline BiquadCoeffs lowpassCoeffs(double hz, double q, double rate) noexcept {
    const auto [c, alpha] = detail::prototype(hz, q, rate);
    return detail::normalised((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

[[nodiscard]] inline BiquadCoeffs highpassCoeffs(double hz, double q, double rate) noexcept {
    const auto [c, alpha] = detail::prototype(hz, q, rate);
    return detail::normalised((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

// Constant 0 dB peak gain band-pass.
[[nodiscard]] inline BiquadCoeffs bandpassCoeffs(double hz, double q, double rate) noexcept {
    const auto [c, alpha] = detail::prototype(hz, q, rate);
    return detail::normalised(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

[[nodiscard]] inline BiquadCoeffs peakCoeffs(double hz, double q, double gainDb, double rate) noexcept {
    const auto [c, alpha] = detail::prototype(hz, q, rate);
    const double a = std::pow(10.0, gainDb / 40.0);
    return detail::normalised(1.0 + alpha * a, -2.0 * c, 1.0 - alpha * a,
                              1.0 + alpha / a, -2.0 * c, 1.0 - alpha / a);
}

// Transposed direct form II. Double state keeps 15 Hz filters stable at 192 kHz.
struct Biquad {
    BiquadCoeffs c{};
    double z1 = 0.0;
    double z2 = 0.0;

    [[nodiscard]] float process(float input) noexcept {
        const double x = input;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return static_cast<float>(y);
    }

    void reset() noexcept {
        z1 = 0.0;
        z2 = 0.0;
    }
};

// Deterministic xorshift noise in [-1, 1].
struct Noise {
    uint32_t state = 0x9E3779B9u;

    [[nodiscard]] float next() noexcept {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return static_cast<float>(state >> 8u) * (2.0f / 16777216.0f) - 1.0f;
    }
};

// Smooth clipper: linear near zero, approaches +/-1. Firmer knee than tanh.
[[nodiscard]] inline float softClip(float x) noexcept {
    const float a = std::abs(x);
    return x / std::pow(1.0f + std::pow(a, 2.5f), 0.4f);
}

// Linear-phase half-band FIR that halves the sample rate.
// Every other tap is zero, so only the centre and odd offsets are stored.
class HalfbandDecimator {
public:
    explicit HalfbandDecimator(int taps, double kaiserBeta);

    void reset() noexcept;
    [[nodiscard]] float process(float first, float second) noexcept;
    [[nodiscard]] int delayInOutputSamples() const noexcept { return (taps_ - 1) / 4; }

private:
    void push(float sample) noexcept;

    int taps_ = 0;
    int position_ = 0;
    std::vector<double> history_;
    std::vector<int> offsets_;
    std::vector<double> weights_;
};

// 1x, 2x, or 4x oversampling decimator built from half-band stages.
class Decimator {
public:
    static constexpr int kMaxFactor = 4;

    void prepare(int factor);
    void reset() noexcept;
    [[nodiscard]] int factor() const noexcept { return factor_; }
    [[nodiscard]] float process(const std::array<float, kMaxFactor>& input) noexcept;

private:
    int factor_ = 1;
    HalfbandDecimator coarse_{ 19, 7.0 };
    HalfbandDecimator fine_{ 47, 7.5 };
};

// The kick voice runs at 176.4 kHz or above so saturation stays clean.
[[nodiscard]] inline int oversamplingFactorFor(double hostRate) noexcept {
    if (hostRate < 60000.0) {
        return 4;
    }
    if (hostRate < 120000.0) {
        return 2;
    }
    return 1;
}

} // namespace openedmkick::dsp
