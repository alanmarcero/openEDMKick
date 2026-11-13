#pragma once

#include "KickDsp.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace openedmkick::core {

enum class Saturation : int { warm = 0, tube, crunch, fold };
enum class ClickType : int { beater = 0, noise, tick, analog };

inline constexpr std::array<std::string_view, 4> kSaturationNames{ "Warm", "Tube", "Crunch", "Fold" };
inline constexpr std::array<std::string_view, 4> kClickTypeNames{ "Beater", "Noise", "Tick", "Analog" };

// Every control that shapes a single hit. Root pitch always comes from MIDI.
struct KickSettings {
    // Pitch envelope, in octaves above the note.
    float sweepOctaves = 2.4f;    // 0..6, main body drop
    float sweepMs = 70.0f;        // 5..400, time to land on the note
    float sweepShape = 0.55f;     // 0 = even glide, 1 = fast dive then slow landing
    float snapOctaves = 2.0f;     // 0..5, extra drop in the first milliseconds
    float snapMs = 2.0f;          // 0.2..12

    // Body harmonics, resonant knock, and saturation.
    float bodyTone = 0.65f;       // 0 round .. 1 harmonic
    float drive = 0.55f;          // 0..1
    Saturation saturation = Saturation::warm;
    float knockHz = 180.0f;       // 60..800, pre-drive emphasis frequency
    float knockDb = 6.0f;         // 0..18
    float grit = 0.35f;           // 0 = tail cleans up, 1 = tail stays saturated
    float toneHz = 9000.0f;       // 300..20000, post-drive low-pass
    float subLevel = 0.35f;       // 0..1, clean phase-locked sine under the body
    float startPhase = 0.5f;      // 0..1 maps to 0..90 degrees

    // Transient layer.
    ClickType clickType = ClickType::beater;
    float clickLevel = 0.45f;     // 0..1
    float clickHz = 3500.0f;      // 500..14000
    float clickMs = 6.0f;         // 0.5..40

    // One-shot amplitude envelope.
    float attackMs = 0.15f;       // 0..10
    float holdMs = 45.0f;         // 0..400
    float decayMs = 300.0f;       // 1..1500
    float decayShape = 0.5f;      // 0 = straight fade, 1 = fast exponential

    // Voice output.
    float lowCutHz = 25.0f;       // 10..90
    float clip = 0.45f;           // 0..1

};

inline constexpr int kDefaultMidiNote = 36; // C2, 65.41 Hz

// Equal-tempered frequency for a MIDI note (A4 = 69 = 440 Hz).
[[nodiscard]] float noteFrequencyHz(int midiNote) noexcept;

// Note name such as "F" and octave such as 1 using the C4 = 60 convention.
[[nodiscard]] std::string_view noteName(int midiNote) noexcept;
[[nodiscard]] int noteOctave(int midiNote) noexcept;

// Shaped fall from 1 at progress 0 to exactly 0 at progress 1. Larger k falls faster at first.
[[nodiscard]] float shapedFall(float progress, float k) noexcept;

// Octaves above the note at a given time after the hit.
[[nodiscard]] float pitchOctavesAt(float timeMs, const KickSettings& settings) noexcept;

// Amplitude envelope (0..1) at a given time after the hit.
[[nodiscard]] float amplitudeAt(float timeMs, const KickSettings& settings) noexcept;

// Total audible duration of one hit, including the transient layer.
[[nodiscard]] float hitLengthMs(const KickSettings& settings) noexcept;

// Waveshaper for the body. Level compensation is applied by the caller.
[[nodiscard]] float saturate(float x, Saturation type) noexcept;

// Input gain into the saturation stage for a Drive amount.
[[nodiscard]] float driveGain(float drive, Saturation type) noexcept;

// One kick voice. Renders at an internal oversampled rate and returns host-rate samples.
class KickVoice {
public:
    void prepare(double hostRate);
    void start(const KickSettings& settings, int midiNote, float gain) noexcept;
    void startReverse(const KickSettings& settings, int midiNote, float gain, int durationSamples) noexcept;
    void choke() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] float nextSample() noexcept;

private:
    void startState(const KickSettings& settings, int midiNote, float gain) noexcept;
    [[nodiscard]] float renderHostSample() noexcept;
    [[nodiscard]] float renderOversampled() noexcept;
    [[nodiscard]] float renderClick(float timeMs) noexcept;

    double hostRate_ = 48000.0;
    double osRate_ = 192000.0;
    dsp::Decimator decimator_;

    KickSettings settings_{};
    bool active_ = false;
    float gain_ = 1.0f;
    float noteHz_ = 43.65f;
    float previousSample_ = 0.0f;
    double phase_ = 0.0;
    double subPhase_ = 0.0;
    double clickPhase_ = 0.0;
    int64_t osIndex_ = 0;
    int64_t hostIndex_ = 0;
    int64_t hostLength_ = 0;
    int64_t chokeRemaining_ = -1;
    int64_t chokeLength_ = 1;
    bool reversing_ = false;
    int reverseSamplesRemaining_ = 0;
    double reverseReadPosition_ = 0.0;
    double reverseReadStep_ = 1.0;
    std::vector<float> reverseBuffer_;

    dsp::Biquad knock_;
    dsp::Biquad tone_;
    dsp::Biquad lowCut_;
    dsp::Biquad clickFilter_;
    dsp::Noise noise_;
};

// Offline render of one hit, sample-identical to live playback of the same note.
[[nodiscard]] std::vector<float> renderKick(const KickSettings& settings, double sampleRate, int midiNote);

} // namespace openedmkick::core
