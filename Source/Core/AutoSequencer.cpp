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
    for (auto& notes : slotNotes_) notes.reserve(kMaxPatternNotes);
    // Factory patterns contain at most one unratcheted hit per cell; custom growth happens in editor snapshots.
    for (auto& hits : scheduledHits_) hits.reserve(kMaxPatternSteps);
    for (int slot = 0; slot < kNumAutoSlots; ++slot) {
        loadPresetIntoSlot(slot, slot, 4);
    }
}

void AutoSequencer::reset() noexcept {
    internalPpq_ = 0.0;
    lastSamplePpq_ = -1.0;
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
    copyNotesForSlot(slot, notes);
}

void AutoSequencer::copyNotesForSlot(int slot, std::span<const PianoRollNote> notes) {
    if (slot < 0 || slot >= kNumAutoSlots) return;
    auto& slotNotes = slotNotes_[static_cast<size_t>(slot)];
    const auto count = std::min(notes.size(), kMaxPatternNotes);
    slotNotes.assign(notes.begin(), notes.begin() + static_cast<std::ptrdiff_t>(count));
    for (auto& note : slotNotes) {
        note.step64 = std::clamp(note.step64, 0, kMaxPatternSteps - 1);
        note.length64 = std::clamp(note.length64, 1, 64);
        note.layer = std::clamp(note.layer, -1, 3);
        note.ratchets = std::clamp(note.ratchets, 1, 8);
        note.velocity = clampUnit(note.velocity);
    }
    std::ranges::sort(slotNotes, {}, &PianoRollNote::step64);
    loadedPreset_[static_cast<size_t>(slot)] = -1;
    rebuildSchedule(slot);
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
    rebuildSchedule(slot);
}

bool AutoSequencer::toggleNoteAt(int slot, int step64, int length64, int totalBars, int layer, int ratchets) {
    if (slot < 0 || slot >= kNumAutoSlots) {
        return false;
    }
    const int maxStep = std::max(1, std::clamp(totalBars, 1, 64)) * kStepsPerBar64;
    if (step64 < 0 || step64 >= maxStep) {
        return false;
    }
    auto& notes = slotNotes_[static_cast<size_t>(slot)];
    const int safeLength = std::clamp(length64, 1, 64);
    const auto found = std::ranges::find_if(notes, [step64, layer](const auto& note) {
        return note.step64 == step64 && note.layer == layer;
    });
    if (found != notes.end()) {
        notes.erase(found);
        loadedPreset_[static_cast<size_t>(slot)] = -1;
        rebuildSchedule(slot);
        return false;
    }
    if (notes.size() >= kMaxPatternNotes) return false;
    notes.push_back({step64, safeLength, 1.0f, false, std::clamp(layer, -1, 3), std::clamp(ratchets, 1, 8)});
    std::ranges::sort(notes, {}, &PianoRollNote::step64);
    loadedPreset_[static_cast<size_t>(slot)] = -1;
    rebuildSchedule(slot);
    return true;
}

void AutoSequencer::swapSlotPattern(int slot, AutoSequencer& other) noexcept {
    if (slot < 0 || slot >= kNumAutoSlots) return;
    const auto index = static_cast<size_t>(slot);
    slotNotes_[index].swap(other.slotNotes_[index]);
    scheduledHits_[index].swap(other.scheduledHits_[index]);
}

void AutoSequencer::cycleRatchetsAt(int slot, int step64, int layer) {
    if (slot < 0 || slot >= kNumAutoSlots) return;
    auto& notes = slotNotes_[static_cast<size_t>(slot)];
    const auto found = std::ranges::find_if(notes, [=](const auto& note) {
        return note.step64 == step64 && note.layer == layer;
    });
    if (found == notes.end()) return;
    found->ratchets = found->ratchets % 8 + 1;
    rebuildSchedule(slot);
}

void AutoSequencer::rebuildSchedule(int slot) {
    auto& hits = scheduledHits_[static_cast<size_t>(slot)];
    const auto& notes = slotNotes_[static_cast<size_t>(slot)];
    hits.clear();
    std::array<int, 5> firstStep{};
    firstStep.fill(kMaxPatternSteps);
    for (const auto& note : notes) {
        const auto lane = static_cast<size_t>(note.layer + 1);
        firstStep[lane] = std::min(firstStep[lane], note.step64);
    }
    std::array<int, 5> nextStep;
    for (size_t lane = 0; lane < firstStep.size(); ++lane) nextStep[lane] = kMaxPatternSteps + firstStep[lane];
    int groupStep = -1;
    std::array<bool, 5> groupLanes{};
    for (auto note = notes.rbegin(); note != notes.rend(); ++note) {
        if (note->step64 != groupStep) {
            for (size_t lane = 0; lane < groupLanes.size(); ++lane)
                if (groupLanes[lane]) nextStep[lane] = groupStep;
            groupLanes.fill(false);
            groupStep = note->step64;
        }
        int next = nextStep[0];
        if (note->layer >= 0) next = std::min(next, nextStep[static_cast<size_t>(note->layer + 1)]);
        if (note->layer < 0) next = *std::min_element(nextStep.begin(), nextStep.end());
        const int ratchets = std::clamp(note->ratchets, 1, 8);
        double spacing = static_cast<double>(note->length64) / ratchets;
        if (ratchets == 1) spacing = std::clamp(next - note->step64, 1, 64);
        if (ratchets == 1 && next >= kMaxPatternSteps) spacing = next - note->step64;
        for (int repeat = 0; repeat < ratchets; ++repeat) {
            hits.push_back({note->step64 + static_cast<double>(repeat * note->length64) / ratchets,
                            spacing, note->velocity, note->layer, note->reverseCue, ratchets == 1 && next >= kMaxPatternSteps});
        }
        groupLanes[static_cast<size_t>(note->layer + 1)] = true;
    }
    std::ranges::sort(hits, [](const auto& left, const auto& right) {
        if (left.step64 != right.step64) return left.step64 < right.step64;
        return left.layer < right.layer;
    });
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
    const double ppqPerSample = safeBpm / (60.0 * safeRate);
    const double previousPpq = lastSamplePpq_;
    const bool sameSlot = slot == lastSlot_;
    const bool repeatedPosition = slot == lastSlot_ && samplePpq == lastSamplePpq_ && windowBeats == lastWindowBeats_;
    lastSlot_ = slot;
    lastStep64_ = step;
    lastSamplePpq_ = samplePpq;
    lastWindowBeats_ = windowBeats;
    if (repeatedPosition) return event;

    const double position64 = cycle * totalSteps / windowBeats;
    const double sampleWidth64 = ppqPerSample * totalSteps / windowBeats;
    const auto& hits = scheduledHits_[static_cast<size_t>(slot)];
    const auto first = std::lower_bound(hits.begin(), hits.end(), position64 - sampleWidth64 + 1e-8,
        [](const auto& hit, double position) { return hit.step64 < position; });
    for (auto found = first; found != hits.end() && found->step64 <= position64 + 1e-8; ++found) {
        if (found->step64 >= totalSteps) continue;
        const double hitPpq = samplePpq - cycle + found->step64 * windowBeats / totalSteps;
        if (sameSlot && previousPpq <= samplePpq && samplePpq - previousPpq <= 2 * ppqPerSample
            && hitPpq <= previousPpq + 1e-10) continue;
        double spacing64 = found->spacing64;
        if (found->wraps) spacing64 += totalSteps - kMaxPatternSteps;
        if (found->wraps) spacing64 = std::clamp(spacing64, 1.0, 64.0);
        const double spacingBeats = spacing64 * windowBeats / totalSteps;
        const float tail = computeAutoEffectiveTailMs(baseTailMs, settings.tailShorten, spacingBeats * 60000.0 / safeBpm);
        event.triggered = true;
        event.velocity = clampUnit(found->velocity);
        event.effectiveTailMs = tail;
        event.reverseCue = found->reverseCue;
        event.stepDurationBeats = spacingBeats;
        for (int layer = 0; layer < 4; ++layer) {
            if (found->layer >= 0 && found->layer != layer) continue;
            event.layerMask |= 1u << layer;
            event.layerVelocities[static_cast<size_t>(layer)] = event.velocity;
            event.layerTailMs[static_cast<size_t>(layer)] = tail;
            event.layerReverseCue[static_cast<size_t>(layer)] = found->reverseCue;
        }
    }
    return event;
}

} // namespace openedmkick::core
