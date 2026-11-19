#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace openedmkick::core {

// A short mono click. Built-ins are synthesized in code. User audio is the same shape.
struct ClickSampleBuffer {
    std::vector<float> samples;
    double sampleRate = 44100.0;
};

enum class BuiltInClick : int { tick = 0, beater, wood, snap, count };

inline constexpr std::array<const char*, 4> kBuiltInClickNames{ "Tick", "Beater", "Wood", "Snap" };
inline constexpr int kUserClickSource = 4;

// Deterministic synthesized clicks. The first call builds them; later calls do not allocate.
void warmBuiltInClicks();
[[nodiscard]] const ClickSampleBuffer& builtInClick(BuiltInClick type);

// Polyphonic one-shot player. trigger, nextSample, and stop do not allocate.
class ClickSamplePlayer {
public:
    void prepare(double hostRate) noexcept;
    void setSource(const float* samples, int length, double sampleRate) noexcept;
    void trigger(float startAmount, float pitchSemitones, float gain, float highPassHz) noexcept;
    void stop() noexcept;
    [[nodiscard]] float nextSample() noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] double remainingSeconds() const noexcept;

private:
    struct Voice {
        bool active = false;
        double position = 0;
        double increment = 1;
        float level = 0;
        uint32_t age = 0;
    };

    [[nodiscard]] float read(Voice& voice) noexcept;

    static constexpr int kVoices = 8;
    std::array<Voice, kVoices> voices_{};
    const float* data_ = nullptr;
    int length_ = 0;
    double sourceRate_ = 44100.0;
    double hostRate_ = 48000.0;
    float hpA_ = 1;
    float hpX_ = 0;
    float hpY_ = 0;
    uint32_t age_ = 0;
};

} // namespace openedmkick::core
