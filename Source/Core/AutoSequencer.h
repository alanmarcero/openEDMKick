#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace openedmkick::core {

inline constexpr int kStepsPerBeat64 = 16; // 16 sixty-fourth notes per quarter-note beat
inline constexpr int kStepsPerBar64 = 64;  // 64 sixty-fourth notes per 4/4 bar
inline constexpr int kNumAutoSlots = 4;
inline constexpr int kNumFillPresets = 28;

inline constexpr std::array<int, 7> kAutoBarCounts = { 1, 2, 4, 8, 16, 32, 64 };

// Grid resolutions for the Piano Roll editor (in 1/64th-note units per grid cell).
// 16 = 1/4 note, 8 = 1/8 note, 4 = 1/16 note, 2 = 1/32 note, 1 = 1/64 note.
inline constexpr std::array<int, 5> kGridStepSizes64 = { 16, 8, 4, 2, 1 };

inline constexpr std::array<std::string_view, kNumFillPresets> kFillPresetNames = {
    "Sunrise Four-Point Lift",
    "Halo Step Ascent",
    "Uplift Sixteenth Surge",
    "Mainstage 138 Switchback",
    "Peak-Time Thirty-Second Finale",
    "Psy Triplet Carousel",
    "Strobe Floor Climb",
    "Skyline Offbeat Push",
    "Last-Light Reverse Pickup",
    "Progressive Meridian Rise",
    "Hard Trance Double-Time",
    "Staccato Drop Stamp",
    "Psy Grid Vector",
    "Afterhours Rolling Accent",
    "144 Gate Step Sequence",
    "Breakdown To Bright",
    "Euphoric Long-Arc Build",
    "Festival Ladder Climb",
    "Forest Machine Pulse",
    "Euro Anthem Bounce",
    "Hard Circuit Sprint",
    "Euphoria Call And Response",
    "Trance Gate Matrix",
    "Vintage Uplift Motion",
    "Full Moon Progressive",
    "Afterburn 64th Cyclone",
    "Infinite Drop Roll",
    "Final Beacon Reverse"
};

struct PianoRollNote {
    int step64 = 0;         // Start position in 1/64-note steps from the start of the pattern
    int length64 = 4;       // Gate length in 1/64-note steps (>= 1)
    int semitoneOffset = 0; // Pitch offset in semitones (-12..+24) relative to tuned kick note
    float velocity = 1.0f;  // 0..1
    bool reverseCue = false;

    bool operator==(const PianoRollNote&) const noexcept = default;
};

struct AutoSlotSettings {
    int bars = 4;              // 1, 2, 4, 8, 16, 32, 64
    int fillPreset = 0;        // Index into the current 28-fill trance bank.
    int gridResolution = 4;    // index into kGridStepSizes64 (default 4 = 1/64 note)
    float tailShorten = 0.65f; // 0..1 (specific to Auto Mode; never affects Regular Mode)
    float pitchRise = 0.0f;    // 0..12 semitones progressive pitch rise across the bar window
    float crushSweep = 0.0f;   // 0..1 late-build bitcrush intensification

    bool operator==(const AutoSlotSettings&) const noexcept = default;
};

struct AutoTriggerEvent {
    bool triggered = false;
    int sampleOffset = 0;
    float velocity = 1.0f;
    float semitoneOffset = 0.0f;
    float effectiveTailMs = -1.0f;
    bool reverseCue = false;
    double stepDurationBeats = 1.0;
};

// Generates the factory EDM kick fill and build-up piano roll patterns scaled to the selected bar count.
[[nodiscard]] std::vector<PianoRollNote> buildPresetPattern(int presetIndex, int bars);

// Computes the shortened kick tail duration in milliseconds for Auto Mode.
// Prevents mud when rapid 1/16, 1/32, and 1/64 notes trigger consecutively.
[[nodiscard]] float computeAutoEffectiveTailMs(
    float baseTailMs,
    float tailShorten,
    double stepSpacingMs) noexcept;

// Manages the 4 independent A/B/C/D Auto Mode piano roll patterns and host-synced playback.
class AutoSequencer {
public:
    AutoSequencer();

    void reset() noexcept;

    [[nodiscard]] const std::vector<PianoRollNote>& notesForSlot(int slot) const noexcept;
    void setNotesForSlot(int slot, std::vector<PianoRollNote> notes);
    void loadPresetIntoSlot(int slot, int presetIndex, int bars);
    bool toggleNoteAt(int slot, int step64, int length64, int semitoneOffset, int totalBars);

    // Advances the clock by one sample and returns a trigger event if a piano roll note starts on this sample.
    [[nodiscard]] AutoTriggerEvent stepSample(
        int activeSlot,
        const AutoSlotSettings& settings,
        float baseTailMs,
        double bpm,
        double beatsPerBar,
        double sampleRate,
        bool hasHostPpq,
        double samplePpq) noexcept;

    [[nodiscard]] float currentWindowPhase() const noexcept { return windowPhase_; }
    [[nodiscard]] int currentStep64() const noexcept { return lastStep64_; }

private:
    std::array<std::vector<PianoRollNote>, kNumAutoSlots> slotNotes_;
    std::array<int, kNumAutoSlots> loadedPreset_{ -1, -1, -1, -1 };
    std::array<int, kNumAutoSlots> loadedBars_{ 0, 0, 0, 0 };
    double internalPpq_ = 0.0;
    int lastStep64_ = -1;
    int lastSlot_ = -1;
    float windowPhase_ = 0.0f;
};

} // namespace openedmkick::core
