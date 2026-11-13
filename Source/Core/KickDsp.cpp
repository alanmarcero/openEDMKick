#include "KickDsp.h"

#include <numeric>
#include <ranges>

namespace openedmkick::dsp {

namespace {

[[nodiscard]] double besselI0(double x) noexcept {
    const double half = x * 0.5;
    double term = 1.0;
    double sum = 1.0;
    std::ranges::for_each(std::views::iota(1, 32), [&](int k) {
        const double ratio = half / static_cast<double>(k);
        term *= ratio * ratio;
        sum += term;
    });
    return sum;
}

[[nodiscard]] double halfbandTap(int index, int taps, double beta) noexcept {
    const int centre = (taps - 1) / 2;
    const int offset = index - centre;
    if (offset == 0) {
        return 0.5;
    }
    if (offset % 2 == 0) {
        return 0.0;
    }
    const double x = std::numbers::pi * 0.5 * static_cast<double>(offset);
    const double sinc = std::sin(x) / x;
    const double ratio = 2.0 * static_cast<double>(index) / static_cast<double>(taps - 1) - 1.0;
    const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - ratio * ratio))) / besselI0(beta);
    return 0.5 * sinc * window;
}

} // namespace

HalfbandDecimator::HalfbandDecimator(int taps, double kaiserBeta)
    : taps_(taps), history_(static_cast<size_t>(taps) * 2u, 0.0) {
    std::ranges::for_each(std::views::iota(0, taps), [&](int index) {
        const double tap = halfbandTap(index, taps, kaiserBeta);
        if (tap == 0.0) {
            return;
        }
        offsets_.push_back(index);
        weights_.push_back(tap);
    });
    const double sum = std::accumulate(weights_.begin(), weights_.end(), 0.0);
    std::ranges::for_each(weights_, [sum](double& weight) { weight /= sum; });
}

void HalfbandDecimator::reset() noexcept {
    std::ranges::fill(history_, 0.0);
    position_ = 0;
}

void HalfbandDecimator::push(float sample) noexcept {
    history_[static_cast<size_t>(position_)] = sample;
    history_[static_cast<size_t>(position_ + taps_)] = sample;
    position_ = (position_ + 1) % taps_;
}

float HalfbandDecimator::process(float first, float second) noexcept {
    push(first);
    push(second);
    const int newest = position_ + taps_ - 1;
    double sum = 0.0;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, offsets_.size()), [&](size_t i) {
        sum += weights_[i] * history_[static_cast<size_t>(newest - offsets_[i])];
    });
    return static_cast<float>(sum);
}

void Decimator::prepare(int factor) {
    factor_ = factor >= 4 ? 4 : (factor >= 2 ? 2 : 1);
    reset();
}

void Decimator::reset() noexcept {
    coarse_.reset();
    fine_.reset();
}

float Decimator::process(const std::array<float, kMaxFactor>& input) noexcept {
    if (factor_ == 1) {
        return input[0];
    }
    if (factor_ == 2) {
        return fine_.process(input[0], input[1]);
    }
    const float first = coarse_.process(input[0], input[1]);
    const float second = coarse_.process(input[2], input[3]);
    return fine_.process(first, second);
}

} // namespace openedmkick::dsp
