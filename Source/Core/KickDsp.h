#pragma once

#include <algorithm>
#include <array>
#include <bit>
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

struct SvfCoeffs {
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double m0 = 1.0;
    double m1 = 0.0;
    double m2 = 0.0;
};

using BiquadCoeffs = SvfCoeffs;

namespace detail {

struct SvfPrototype {
    double g = 0.0;
    double k = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
};

[[nodiscard]] inline SvfPrototype svfPrototype(double hz, double q, double rate) noexcept {
    const double safeRate = std::max(1000.0, rate);
    const double limited = std::clamp(hz, 5.0, safeRate * 0.49);
    const double g = std::tan(std::numbers::pi * limited / safeRate);
    const double k = 1.0 / std::max(0.05, q);
    const double a1 = 1.0 / (1.0 + g * (g + k));
    const double a2 = g * a1;
    return { g, k, a1, a2, g * a2 };
}

} // namespace detail

[[nodiscard]] inline SvfCoeffs lowpassCoeffs(double hz, double q, double rate) noexcept {
    const auto p = detail::svfPrototype(hz, q, rate);
    return { p.a1, p.a2, p.a3, 0.0, 0.0, 1.0 };
}

[[nodiscard]] inline SvfCoeffs highpassCoeffs(double hz, double q, double rate) noexcept {
    const auto p = detail::svfPrototype(hz, q, rate);
    return { p.a1, p.a2, p.a3, 1.0, -p.k, -1.0 };
}

// Constant 0 dB peak gain band-pass.
[[nodiscard]] inline SvfCoeffs bandpassCoeffs(double hz, double q, double rate) noexcept {
    const auto p = detail::svfPrototype(hz, q, rate);
    return { p.a1, p.a2, p.a3, 0.0, p.k, 0.0 };
}

[[nodiscard]] inline SvfCoeffs peakCoeffs(double hz, double q, double gainDb, double rate) noexcept {
    const double a = std::pow(10.0, gainDb / 40.0);
    const auto p = detail::svfPrototype(hz, std::max(0.05, q) * a, rate);
    return { p.a1, p.a2, p.a3, 1.0, p.k * (a * a - 1.0), 0.0 };
}

// Double state keeps 15 Hz filters clean at 192 kHz.
struct Svf {
    SvfCoeffs c{};
    double ic1eq = 0.0;
    double ic2eq = 0.0;

    [[nodiscard]] float process(float input) noexcept {
        const double v0 = input;
        const double v3 = v0 - ic2eq;
        const double v1 = c.a1 * ic1eq + c.a2 * v3;
        const double v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        return static_cast<float>(c.m0 * v0 + c.m1 * v1 + c.m2 * v2);
    }

    void reset() noexcept {
        ic1eq = 0.0;
        ic2eq = 0.0;
    }
};

using Biquad = Svf;

struct Noise {
    uint32_t state = 0x9E3779B9u;
    uint32_t counter = 0u;
    std::array<float, 5> pinkRows{};
    float pinkSum = 0.0f;
    int velvetCountdown = 0;
    int velvetOffset = 0;
    float velvetSign = 1.0f;

    void seed(uint32_t value) noexcept {
        state = value != 0u ? value : 0x9E3779B9u;
        counter = 0u;
        pinkRows.fill(0.0f);
        pinkSum = 0.0f;
        velvetCountdown = 0;
        velvetOffset = 0;
        velvetSign = 1.0f;
    }

    [[nodiscard]] float next() noexcept {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return static_cast<float>(state >> 8u) * (2.0f / 16777216.0f) - 1.0f;
    }

    [[nodiscard]] float nextPink() noexcept {
        ++counter;
        const auto row = static_cast<size_t>(std::countr_zero(counter));
        if (row < pinkRows.size()) {
            const float fresh = next();
            pinkSum += fresh - pinkRows[row];
            pinkRows[row] = fresh;
        }
        return (pinkSum + next()) * (1.0f / 6.0f);
    }

    [[nodiscard]] float nextVelvet(double densityHz, double sampleRate) noexcept {
        if (velvetCountdown <= 0) {
            const double safeDensity = std::max(50.0, densityHz);
            const int cell = std::clamp(static_cast<int>(std::lround(sampleRate / safeDensity)), 2, 4096);
            velvetCountdown = cell;
            const float uniform = 0.5f * (next() + 1.0f);
            velvetOffset = std::clamp(static_cast<int>(uniform * static_cast<float>(cell)), 0, cell - 1);
            velvetSign = next() >= 0.0f ? 1.0f : -1.0f;
        }
        --velvetCountdown;
        if (velvetCountdown == velvetOffset) return velvetSign;
        return 0.0f;
    }
};

// Subtracting tanh(bias) and scaling by the bias derivative keeps silence at zero and small-signal gain at one.
struct AdaaSaturator {
    double prevU = 0.0;
    bool primed = false;

    [[nodiscard]] static double antiderivative(double u) noexcept {
        const double a = std::abs(u);
        return a + std::log1p(std::exp(-2.0 * a)) - std::numbers::ln2;
    }

    [[nodiscard]] float process(float input, float drive, float bias) noexcept {
        const double d = std::max(0.1, static_cast<double>(drive));
        const double b = static_cast<double>(bias);
        const double u = d * static_cast<double>(input) + b;
        if (!primed) {
            primed = true;
            prevU = u;
        }
        const double previous = prevU;
        prevU = u;
        const double du = u - previous;
        double shaped = std::tanh(0.5 * (u + previous));
        if (std::abs(du) > 0.015) shaped = (antiderivative(u) - antiderivative(previous)) / du;
        const double zeroLevel = std::tanh(b);
        const double smallSignalSlope = d * std::max(0.1, 1.0 - zeroLevel * zeroLevel);
        return static_cast<float>((shaped - zeroLevel) / smallSignalSlope);
    }

    void reset() noexcept {
        prevU = 0.0;
        primed = false;
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
