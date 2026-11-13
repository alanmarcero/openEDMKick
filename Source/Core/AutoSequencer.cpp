#include "AutoSequencer.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace openedmkick::core {

namespace {

struct PatternShape {
    int baseDivision;
    int lateDivision;
    int accent;
    int variation;
    bool reverseEnding;
};

constexpr std::array<PatternShape, kNumFillPresets> kShapes{{
    {16, 8, 3, 0, true}, {16, 4, 1, 1, true}, {8, 4, 2, 2, false}, {16, 4, 0, 3, true},
    {8, 2, 3, 4, true}, {12, 3, 1, 5, false}, {16, 4, 2, 6, true}, {8, 4, 0, 7, false},
    {16, 4, 3, 8, true}, {16, 8, 1, 9, true}, {8, 2, 2, 10, false}, {4, 2, 3, 11, true},
    {12, 3, 0, 12, false}, {8, 4, 1, 13, false}, {4, 2, 2, 14, true}, {16, 4, 3, 15, true},
    {16, 8, 1, 16, true}, {8, 2, 0, 17, true}, {12, 3, 2, 18, false}, {8, 4, 3, 19, false},
    {4, 1, 1, 20, true}, {16, 4, 2, 21, false}, {8, 2, 3, 22, true}, {16, 8, 0, 23, false},
    {8, 2, 1, 24, true}, {4, 1, 3, 25, true}, {4, 1, 2, 26, false}, {8, 2, 3, 27, true}
}};

constexpr std::array<int, 7> kPresetBarChoices{ 1, 2, 4, 8, 16, 32, 64 };

using PresetPatternBank = std::array<
    std::array<std::vector<PianoRollNote>, kPresetBarChoices.size()>, kNumFillPresets>;

const PresetPatternBank& presetPatternBank() {
    static const PresetPatternBank patterns = [] {
        PresetPatternBank result;
        for (int preset = 0; preset < kNumFillPresets; ++preset) {
            for (size_t barChoice = 0; barChoice < kPresetBarChoices.size(); ++barChoice) {
                result[static_cast<size_t>(preset)][barChoice] =
                    buildPresetPattern(preset, kPresetBarChoices[barChoice]);
            }
        }
        return result;
    }();
    return patterns;
}

[[nodiscard]] float clampUnit(float value) noexcept {
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 1.0f);
}

[[nodiscard]] bool hasNoteAt(const std::vector<PianoRollNote>& notes, int step) noexcept {
    return std::ranges::any_of(notes, [step](const auto& note) { return note.step64 == step; });
}

} // namespace

std::vector<PianoRollNote> buildPresetPattern(int presetIndex, int bars) {
    const int safePreset = std::clamp(presetIndex, 0, kNumFillPresets - 1);
    const int safeBars = std::clamp(bars, 1, 64);
    const auto shape = kShapes[static_cast<size_t>(safePreset)];
    std::vector<PianoRollNote> result;
    result.reserve(static_cast<size_t>(safeBars) * 24u);

    for (int bar = 0; bar < safeBars; ++bar) {
        const bool finalBar = bar == safeBars - 1;
        const int localStart = bar * kStepsPerBar64;
        const int division = finalBar ? shape.lateDivision : shape.baseDivision;
        const int stride = std::max(1, division);
        const int phaseShift = (shape.variation * 3 + bar * (1 + shape.variation % 5)) % stride;
        const int finalQuarterStart = localStart + 48;

        // A grounded four-on-the-floor anchor gives the build a clear pulse.
        for (int beat = 0; beat < 4; ++beat) {
            const int step = localStart + beat * 16;
            const bool keepAnchor = ((shape.variation + beat + bar) % 5) != 1 || !finalBar;
            if (keepAnchor && !hasNoteAt(result, step)) {
                result.push_back({step, std::min(12, stride), beat == 0 ? 1.0f : 0.82f, false});
            }
        }

        for (int step = localStart + phaseShift; step < localStart + kStepsPerBar64; step += stride) {
            if (step < 0 || step >= safeBars * kStepsPerBar64 || hasNoteAt(result, step)) {
                continue;
            }
            const int local = step - localStart;
            const bool buildZone = local >= 32 || finalBar;
            if (!buildZone && ((local / stride + shape.variation) % 3 != 0)) {
                continue;
            }
            const float velocity = std::clamp(0.52f + 0.06f * static_cast<float>((local / stride + shape.accent) % 6), 0.48f, 0.92f);
            const int length = std::clamp(stride - stride / 4, 1, 16);
            result.push_back({step, length, velocity,
                              shape.reverseEnding && finalBar && step >= finalQuarterStart && (step + shape.variation) % (2 * stride) == 0});
        }

        if (shape.variation % 4 == 0 || shape.variation % 4 == 3) {
            const int pickup = localStart + 60 + (shape.variation % 4);
            if (pickup < localStart + kStepsPerBar64 && !hasNoteAt(result, pickup)) {
                result.push_back({pickup, 2, 0.88f, shape.reverseEnding && finalBar});
            }
        }
    }

    std::ranges::sort(result, {}, &PianoRollNote::step64);
    return result;
}

float computeAutoEffectiveTailMs(float baseTailMs, float tailShorten, double stepSpacingMs) noexcept {
    if (!std::isfinite(baseTailMs) || !std::isfinite(stepSpacingMs)) {
        return 30.0f;
    }
    const float safeBase = std::clamp(baseTailMs, 30.0f, 1200.0f);
    const float amount = clampUnit(tailShorten);
    const double safeSpacing = std::clamp(stepSpacingMs, 15.0, 12000.0);
    const double spacingLimit = safeSpacing * (1.0 - 0.76 * amount);
    const double blended = safeBase + amount * (spacingLimit - safeBase);
    return static_cast<float>(std::clamp(blended, 15.0, static_cast<double>(safeBase)));
}

AutoSequencer::AutoSequencer() {
    for (auto& notes : slotNotes_) notes.reserve(kStepsPerBar64 * 64u);
    for (int slot = 0; slot < kNumAutoSlots; ++slot) {
        loadPresetIntoSlot(slot, slot, 4);
    }
}

void AutoSequencer::reset() noexcept {
    internalPpq_ = 0.0;
    lastStep64_ = -1;
    lastSlot_ = -1;
    windowPhase_ = 0.0f;
}

const std::vector<PianoRollNote>& AutoSequencer::notesForSlot(int slot) const noexcept {
    static const std::vector<PianoRollNote> empty;
    if (slot < 0 || slot >= kNumAutoSlots) {
        return empty;
    }
    return slotNotes_[static_cast<size_t>(slot)];
}

void AutoSequencer::setNotesForSlot(int slot, std::vector<PianoRollNote> notes) {
    if (slot < 0 || slot >= kNumAutoSlots) {
        return;
    }
    std::ranges::sort(notes, {}, &PianoRollNote::step64);
    auto& slotNotes = slotNotes_[static_cast<size_t>(slot)];
    slotNotes.assign(notes.begin(), notes.end());
    loadedPreset_[static_cast<size_t>(slot)] = -1;
}

void AutoSequencer::loadPresetIntoSlot(int slot, int presetIndex, int bars) {
    if (slot < 0 || slot >= kNumAutoSlots) {
        return;
    }
    const int safePreset = std::clamp(presetIndex, 0, kNumFillPresets - 1);
    const int safeBars = std::clamp(bars, 1, 64);
    const auto index = static_cast<size_t>(slot);
    const auto barChoice = std::ranges::find(kPresetBarChoices, safeBars);
    if (barChoice != kPresetBarChoices.end()) {
        const auto presetIndexSafe = static_cast<size_t>(safePreset);
        const auto barChoiceIndex = static_cast<size_t>(std::distance(kPresetBarChoices.begin(), barChoice));
        const auto& pattern = presetPatternBank()[presetIndexSafe][barChoiceIndex];
        slotNotes_[index].assign(pattern.begin(), pattern.end());
    } else {
        auto pattern = buildPresetPattern(safePreset, safeBars);
        slotNotes_[index].assign(pattern.begin(), pattern.end());
    }
    loadedPreset_[index] = safePreset;
    loadedBars_[index] = safeBars;
}

bool AutoSequencer::toggleNoteAt(int slot, int step64, int length64, int totalBars) {
    if (slot < 0 || slot >= kNumAutoSlots) {
        return false;
    }
    const int maxStep = std::max(1, std::clamp(totalBars, 1, 64)) * kStepsPerBar64;
    if (step64 < 0 || step64 >= maxStep) {
        return false;
    }
    auto& notes = slotNotes_[static_cast<size_t>(slot)];
    const int safeLength = std::clamp(length64, 1, 64);
    const auto found = std::ranges::find_if(notes, [step64](const auto& note) {
        return note.step64 == step64;
    });
    if (found != notes.end()) {
        notes.erase(found);
        loadedPreset_[static_cast<size_t>(slot)] = -1;
        return false;
    }
    notes.push_back({step64, safeLength, 1.0f, false});
    std::ranges::sort(notes, {}, &PianoRollNote::step64);
    loadedPreset_[static_cast<size_t>(slot)] = -1;
    return true;
}

AutoTriggerEvent AutoSequencer::stepSample(
    int activeSlot,
    const AutoSlotSettings& settings,
    float baseTailMs,
    double bpm,
    double beatsPerBar,
    double sampleRate,
    bool hasHostPpq,
    double samplePpq) noexcept {
    AutoTriggerEvent event;
    const int slot = std::clamp(activeSlot, 0, kNumAutoSlots - 1);
    const double safeBpm = std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
    const double safeRate = std::max(8000.0, sampleRate);
    const double safeBeatsPerBar = std::clamp(std::isfinite(beatsPerBar) ? beatsPerBar : 4.0, 1.0, 16.0);
    const int bars = std::clamp(settings.bars, 1, 64);
    if (!hasHostPpq) {
        samplePpq = internalPpq_;
        internalPpq_ += safeBpm / (60.0 * safeRate);
    } else {
        internalPpq_ = samplePpq;
    }
    samplePpq = std::max(0.0, samplePpq);

    const double windowBeats = safeBeatsPerBar * bars;
    const double cycle = std::fmod(samplePpq, windowBeats);
    windowPhase_ = static_cast<float>(cycle / windowBeats);
    const int totalSteps = bars * kStepsPerBar64;
    const int step = std::clamp(static_cast<int>(std::floor(windowPhase_ * totalSteps + 1.0e-8)), 0, totalSteps - 1);
    if (slot != lastSlot_) {
        lastSlot_ = slot;
        lastStep64_ = -1;
    }
    if (step == lastStep64_) {
        return event;
    }
    lastStep64_ = step;

    const auto& notes = slotNotes_[static_cast<size_t>(slot)];
    const auto found = std::ranges::find_if(notes, [step](const auto& note) { return note.step64 == step; });
    if (found == notes.end()) {
        return event;
    }
    const auto next = std::next(found);
    const int nextStep = next != notes.end() ? next->step64 : totalSteps + notes.front().step64;
    const int spacing64 = std::clamp(nextStep - found->step64, 1, kStepsPerBar64);
    const double stepSpacingMs = (static_cast<double>(spacing64) / kStepsPerBeat64) * 60000.0 / safeBpm;
    event.triggered = true;
    event.velocity = clampUnit(found->velocity);
    event.effectiveTailMs = computeAutoEffectiveTailMs(baseTailMs, settings.tailShorten, stepSpacingMs);
    event.reverseCue = found->reverseCue;
    event.stepDurationBeats = static_cast<double>(spacing64) / kStepsPerBeat64;
    return event;
}

} // namespace openedmkick::core
