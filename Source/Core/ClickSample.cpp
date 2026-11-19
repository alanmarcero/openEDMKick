#include "ClickSample.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace openedmkick::core {
namespace {

[[nodiscard]] float noise(uint32_t& state) noexcept {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(state)) * (1.0f / 2147483648.0f);
}

[[nodiscard]] ClickSampleBuffer synthesize(uint32_t seed, double seconds, auto&& render) {
    const int count = std::clamp(static_cast<int>(std::lround(44100.0 * seconds)), 32, 2048);
    ClickSampleBuffer buffer;
    buffer.sampleRate = 44100.0;
    buffer.samples.resize(static_cast<size_t>(count));
    uint32_t state = seed;
    float previous = 0.0f;
    float peak = 0.0f;
    for (int index = 0; index < count; ++index) {
        const float time = static_cast<float>(index) / 44100.0f;
        const float white = noise(state);
        const float sample = render(time, white, previous);
        previous = white;
        buffer.samples[static_cast<size_t>(index)] = sample;
        peak = std::max(peak, std::abs(sample));
    }
    const float gain = peak > 1.0e-6f ? 0.85f / peak : 0.0f;
    const int fade = std::min(24, count / 6);
    for (int index = 0; index < count; ++index) {
        float window = 1.0f;
        if (index < fade) window = static_cast<float>(index) / static_cast<float>(fade);
        if (index > count - fade)
            window = std::min(window, static_cast<float>(count - 1 - index) / static_cast<float>(fade));
        buffer.samples[static_cast<size_t>(index)] *= gain * window;
    }
    return buffer;
}

struct Bank {
    std::array<ClickSampleBuffer, 4> sounds;
};

[[nodiscard]] Bank makeBank() {
    Bank bank;
    bank.sounds[0] = synthesize(0x71c4u, 0.012, [](float time, float white, float) {
        return std::sin(2.0f * std::numbers::pi_v<float> * 5200.0f * time) * std::exp(-time * 320.0f)
            + 0.08f * white * std::exp(-time * 500.0f);
    });
    bank.sounds[1] = synthesize(0xbea7u, 0.020, [](float time, float white, float) {
        const float thump = std::sin(2.0f * std::numbers::pi_v<float> * 185.0f * time) * std::exp(-time * 90.0f);
        return white * std::exp(-time * 160.0f) + 0.55f * thump;
    });
    bank.sounds[2] = synthesize(0x0d00u, 0.026, [](float time, float, float) {
        const float body = std::sin(2.0f * std::numbers::pi_v<float> * 680.0f * time) * std::exp(-time * 62.0f);
        const float knock = std::sin(2.0f * std::numbers::pi_v<float> * 1360.0f * time) * std::exp(-time * 140.0f);
        return body + 0.4f * knock;
    });
    bank.sounds[3] = synthesize(0x5aa9u, 0.008, [](float time, float white, float previous) {
        return (white - previous) * std::exp(-time * 620.0f);
    });
    return bank;
}

[[nodiscard]] const Bank& bank() {
    static const Bank sounds = makeBank();
    return sounds;
}

} // namespace

void warmBuiltInClicks() {
    (void)bank();
}

const ClickSampleBuffer& builtInClick(BuiltInClick type) {
    const auto index = static_cast<size_t>(std::clamp(static_cast<int>(type), 0, 3));
    return bank().sounds[index];
}

void ClickSamplePlayer::prepare(double hostRate) noexcept {
    hostRate_ = std::max(8000.0, hostRate);
    stop();
}

void ClickSamplePlayer::setSource(const float* samples, int length, double sampleRate) noexcept {
    if (samples == data_ && length == length_ && sampleRate == sourceRate_) return;
    data_ = samples;
    length_ = std::max(0, length);
    sourceRate_ = sampleRate > 1.0 ? sampleRate : hostRate_;
    stop();
}

void ClickSamplePlayer::stop() noexcept {
    for (auto& voice : voices_) voice.active = false;
    hpX_ = hpY_ = 0.0f;
}

void ClickSamplePlayer::trigger(float startAmount, float pitchSemitones, float gain, float highPassHz) noexcept {
    if (data_ == nullptr || length_ < 2 || !(gain > 0.00001f) || hostRate_ <= 0.0) return;
    Voice* slot = nullptr;
    for (auto& voice : voices_) {
        if (!voice.active) { slot = &voice; break; }
    }
    if (slot == nullptr) {
        slot = &*std::min_element(voices_.begin(), voices_.end(),
            [](const Voice& left, const Voice& right) { return left.age < right.age; });
    }
    const double start = std::clamp(static_cast<double>(startAmount), 0.0, 1.0) * static_cast<double>(length_ - 1);
    const double semis = std::clamp(static_cast<double>(pitchSemitones), -48.0, 48.0);
    const double ratio = std::exp2(semis / 12.0);
    slot->active = true;
    slot->position = start;
    slot->increment = std::max(1.0e-4, (sourceRate_ / hostRate_) * ratio);
    slot->level = gain;
    slot->age = ++age_;
    const float nyquist = static_cast<float>(hostRate_ * 0.45);
    const float cutoff = std::clamp(highPassHz, 10.0f, nyquist);
    hpA_ = std::exp(-2.0f * std::numbers::pi_v<float> * cutoff / static_cast<float>(hostRate_));
}

float ClickSamplePlayer::read(Voice& voice) noexcept {
    if (!voice.active || data_ == nullptr || length_ < 2) return 0.0f;
    if (voice.position >= static_cast<double>(length_ - 1)) {
        voice.active = false;
        return 0.0f;
    }
    const int index = std::clamp(static_cast<int>(voice.position), 0, length_ - 2);
    const float fraction = static_cast<float>(voice.position - static_cast<double>(index));
    const float sample = data_[index] + (data_[index + 1] - data_[index]) * fraction;
    voice.position += voice.increment;
    if (voice.position >= static_cast<double>(length_ - 1)) voice.active = false;
    return sample * voice.level;
}

float ClickSamplePlayer::nextSample() noexcept {
    float sum = 0.0f;
    bool any = false;
    for (auto& voice : voices_) {
        if (!voice.active) continue;
        any = true;
        sum += read(voice);
    }
    if (!any) {
        hpX_ = hpY_ = 0.0f;
        return 0.0f;
    }
    const float filtered = hpA_ * (hpY_ + sum - hpX_);
    hpX_ = sum;
    hpY_ = std::isfinite(filtered) ? filtered : 0.0f;
    if (std::abs(hpY_) < 1.0e-20f) hpY_ = 0.0f;
    return hpY_;
}

bool ClickSamplePlayer::isActive() const noexcept {
    return std::ranges::any_of(voices_, [](const Voice& voice) { return voice.active; });
}

double ClickSamplePlayer::remainingSeconds() const noexcept {
    double left = 0.0;
    if (length_ < 2 || hostRate_ <= 0.0) return 0.0;
    for (const auto& voice : voices_) {
        if (!voice.active) continue;
        const double samples = std::max(0.0, static_cast<double>(length_ - 1) - voice.position);
        left = std::max(left, samples / (voice.increment * hostRate_));
    }
    return left;
}

} // namespace openedmkick::core
