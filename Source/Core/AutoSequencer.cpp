#include "AutoSequencer.h"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace openedmkick::core {

namespace {

void addNote(
    std::vector<PianoRollNote>& notes,
    int step64,
    int length64,
    int semitoneOffset = 0,
    float velocity = 1.0f,
    bool reverseCue = false) {
    notes.push_back(PianoRollNote{
        std::max(0, step64),
        std::max(1, length64),
        std::clamp(semitoneOffset, -12, 24),
        std::clamp(velocity, 0.1f, 1.2f),
        reverseCue
    });
}

void sortAndDeduplicate(std::vector<PianoRollNote>& notes) {
    std::ranges::sort(notes, [](const PianoRollNote& a, const PianoRollNote& b) {
        return a.step64 < b.step64;
    });
    const auto duplicateRange = std::ranges::unique(notes, [](const PianoRollNote& a, const PianoRollNote& b) {
        return a.step64 == b.step64;
    });
    notes.erase(duplicateRange.begin(), duplicateRange.end());
}

void appendSteppedRange(
    std::vector<PianoRollNote>& notes,
    int startStep64,
    int endStep64,
    int stepSize64,
    int pitchStart = 0,
    int pitchEnd = 0,
    float velStart = 0.9f,
    float velEnd = 1.0f) {
    const int safeStep = std::max(1, stepSize64);
    const int span = std::max(0, endStep64 - startStep64);
    const int count = span / safeStep;
    std::ranges::for_each(std::views::iota(0, count), [&](int i) {
        const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
        const int pitch = static_cast<int>(std::round(static_cast<float>(pitchStart) + t * static_cast<float>(pitchEnd - pitchStart)));
        const float vel = velStart + t * (velEnd - velStart);
        addNote(notes, startStep64 + i * safeStep, safeStep, pitch, vel, false);
    });
}

} // namespace

std::vector<PianoRollNote> buildPresetPattern(int presetIndex, int bars) {
    const int safeBars = std::clamp(bars, 1, 64);
    const int totalSteps = safeBars * kStepsPerBar64;
    const int safePreset = ((presetIndex % kNumFillPresets) + kNumFillPresets) % kNumFillPresets;
    std::vector<PianoRollNote> notes;

    switch (safePreset) {
        case 0: { // 0: Four on the Floor + End Fill (1/4 notes, last beat 1/16 + 1/32 turnaround)
            const int fillStart = std::max(0, totalSteps - 16);
            appendSteppedRange(notes, 0, fillStart, 16, 0, 0, 1.0f, 1.0f);
            appendSteppedRange(notes, fillStart, totalSteps - 4, 4, 0, 2, 0.85f, 1.0f);
            appendSteppedRange(notes, totalSteps - 4, totalSteps, 2, 3, 5, 0.95f, 1.0f);
            break;
        }
        case 1: { // 1: Classic EDM Build (1/4 -> 1/8 -> 1/16 -> 1/32)
            const int q1 = totalSteps / 4;
            const int q2 = totalSteps / 2;
            const int q3 = (totalSteps * 3) / 4;
            appendSteppedRange(notes, 0, q1, 16, 0, 0, 0.95f, 1.0f);
            appendSteppedRange(notes, q1, q2, 8, 0, 2, 0.9f, 1.0f);
            appendSteppedRange(notes, q2, q3, 4, 2, 5, 0.85f, 1.0f);
            appendSteppedRange(notes, q3, totalSteps, 2, 5, 12, 0.8f, 1.0f);
            break;
        }
        case 2: { // 2: Festival 1/64 Drop Rush (accelerates all the way down to 1/64 notes!)
            const int p1 = totalSteps / 2;
            const int p2 = (totalSteps * 3) / 4;
            const int p3 = std::max(p2, totalSteps - 16);
            appendSteppedRange(notes, 0, p1, 8, 0, 2, 0.95f, 1.0f);
            appendSteppedRange(notes, p1, p2, 4, 2, 5, 0.9f, 1.0f);
            appendSteppedRange(notes, p2, p3, 2, 5, 8, 0.85f, 1.0f);
            appendSteppedRange(notes, p3, totalSteps, 1, 8, 12, 0.8f, 1.0f);
            break;
        }
        case 3: { // 3: Tech House Groove & Turnaround
            std::ranges::for_each(std::views::iota(0, safeBars), [&](int bar) {
                const int base = bar * kStepsPerBar64;
                addNote(notes, base + 0, 16, 0, 1.0f);
                addNote(notes, base + 16, 16, 0, 0.95f);
                addNote(notes, base + 28, 4, 0, 0.78f);
                addNote(notes, base + 32, 16, 0, 1.0f);
                addNote(notes, base + 48, 8, 0, 0.95f);
                addNote(notes, base + 56, 4, 2, 0.88f);
                addNote(notes, base + 60, 4, 4, 0.92f);
            });
            break;
        }
        case 4: { // 4: Big Room Drop Countdown
            const int buildSplit = std::max(16, totalSteps - 32);
            appendSteppedRange(notes, 0, buildSplit, 16, 0, 3, 1.0f, 1.0f);
            appendSteppedRange(notes, buildSplit, totalSteps - 8, 4, 3, 7, 0.9f, 1.0f);
            appendSteppedRange(notes, totalSteps - 8, totalSteps, 1, 7, 12, 0.85f, 1.0f);
            break;
        }
        case 5: { // 5: Hardstyle Pitch-Up Build
            const int mid = totalSteps / 2;
            appendSteppedRange(notes, 0, mid, 8, 0, 5, 1.0f, 1.0f);
            appendSteppedRange(notes, mid, totalSteps - 8, 4, 5, 10, 0.95f, 1.0f);
            appendSteppedRange(notes, totalSteps - 8, totalSteps, 2, 10, 12, 1.0f, 1.0f);
            break;
        }
        case 6: { // 6: Future Rave Syncopated Fill
            std::ranges::for_each(std::views::iota(0, safeBars), [&](int bar) {
                const int base = bar * kStepsPerBar64;
                addNote(notes, base + 0, 12, 0, 1.0f);
                addNote(notes, base + 12, 12, 0, 0.92f);
                addNote(notes, base + 24, 8, 2, 0.95f);
                addNote(notes, base + 32, 12, 0, 1.0f);
                addNote(notes, base + 44, 12, 3, 0.92f);
                addNote(notes, base + 56, 4, 5, 0.95f);
                addNote(notes, base + 60, 4, 7, 1.0f);
            });
            break;
        }
        case 7: { // 7: Halftime to 1/32 Blaster
            const int half = totalSteps / 2;
            appendSteppedRange(notes, 0, half, 32, 0, 0, 1.0f, 1.0f);
            appendSteppedRange(notes, half, (totalSteps * 3) / 4, 8, 0, 4, 0.9f, 1.0f);
            appendSteppedRange(notes, (totalSteps * 3) / 4, totalSteps, 2, 4, 9, 0.85f, 1.0f);
            break;
        }
        case 8: { // 8: Beat 4 Reverse Pickup Loop (Beats 1-3 forward, Beat 4 reverse kick!)
            std::ranges::for_each(std::views::iota(0, safeBars), [&](int bar) {
                const int base = bar * kStepsPerBar64;
                addNote(notes, base + 0, 16, 0, 1.0f, false);
                addNote(notes, base + 16, 16, 0, 1.0f, false);
                addNote(notes, base + 32, 16, 0, 1.0f, false);
                addNote(notes, base + 48, 16, 0, 1.0f, true);
            });
            break;
        }
        case 9: { // 9: Accelerating Exponential Roll
            const int s1 = totalSteps / 3;
            const int s2 = (totalSteps * 2) / 3;
            const int s3 = std::max(s2, totalSteps - 12);
            appendSteppedRange(notes, 0, s1, 16, 0, 1, 0.95f, 1.0f);
            appendSteppedRange(notes, s1, s2, 8, 1, 4, 0.92f, 1.0f);
            appendSteppedRange(notes, s2, s3, 2, 4, 8, 0.88f, 1.0f);
            appendSteppedRange(notes, s3, totalSteps, 1, 8, 12, 0.85f, 1.0f);
            break;
        }
        case 10: { // 10: Offbeat Stomp & 1/64 Finale
            const int endBurst = std::max(0, totalSteps - 16);
            std::ranges::for_each(std::views::iota(0, endBurst / 16), [&](int beat) {
                const int pos = beat * 16;
                addNote(notes, pos, 8, 0, 1.0f);
                addNote(notes, pos + 8, 8, 2, 0.85f);
            });
            appendSteppedRange(notes, endBurst, totalSteps, 1, 3, 12, 0.8f, 1.0f);
            break;
        }
        case 11: { // 11: Mainstage 64th Machine Gun
            const int intro = std::min(16, totalSteps / 2);
            appendSteppedRange(notes, 0, intro, 4, 0, 3, 0.95f, 1.0f);
            appendSteppedRange(notes, intro, totalSteps, 1, 3, 12, 0.82f, 1.0f);
            break;
        }
    }

    sortAndDeduplicate(notes);
    return notes;
}

float computeAutoEffectiveTailMs(
    float baseTailMs,
    float tailShorten,
    double stepSpacingMs) noexcept {
    const float clampedShorten = std::clamp(tailShorten, 0.0f, 1.0f);
    if (clampedShorten <= 0.0001f) {
        return baseTailMs;
    }
    const float safeSpacingMs = static_cast<float>(std::max(10.0, stepSpacingMs));
    const float scaledTail = baseTailMs * (1.0f - 0.82f * clampedShorten);
    const float antiMudLimit = safeSpacingMs * (1.0f - 0.38f * clampedShorten);
    const float targetTail = std::min(scaledTail, antiMudLimit);
    const float blended = baseTailMs + clampedShorten * (targetTail - baseTailMs);
    return std::clamp(blended, 15.0f, baseTailMs);
}

AutoSequencer::AutoSequencer() {
    std::ranges::for_each(std::views::iota(0, kNumAutoSlots), [this](int slot) {
        loadPresetIntoSlot(slot, slot % kNumFillPresets, 4);
    });
}

void AutoSequencer::reset() noexcept {
    internalPpq_ = 0.0;
    lastStep64_ = -1;
    lastSlot_ = -1;
    windowPhase_ = 0.0f;
}

const std::vector<PianoRollNote>& AutoSequencer::notesForSlot(int slot) const noexcept {
    const int safeSlot = std::clamp(slot, 0, kNumAutoSlots - 1);
    return slotNotes_[static_cast<size_t>(safeSlot)];
}

void AutoSequencer::setNotesForSlot(int slot, std::vector<PianoRollNote> notes) {
    const int safeSlot = std::clamp(slot, 0, kNumAutoSlots - 1);
    sortAndDeduplicate(notes);
    slotNotes_[static_cast<size_t>(safeSlot)] = std::move(notes);
}

void AutoSequencer::loadPresetIntoSlot(int slot, int presetIndex, int bars) {
    const int safeSlot = std::clamp(slot, 0, kNumAutoSlots - 1);
    const int safeBars = std::clamp(bars, 1, 64);
    slotNotes_[static_cast<size_t>(safeSlot)] = buildPresetPattern(presetIndex, safeBars);
    loadedPreset_[static_cast<size_t>(safeSlot)] = presetIndex;
    loadedBars_[static_cast<size_t>(safeSlot)] = safeBars;
}

bool AutoSequencer::toggleNoteAt(int slot, int step64, int length64, int semitoneOffset, int totalBars) {
    const int safeSlot = std::clamp(slot, 0, kNumAutoSlots - 1);
    const int maxStep = std::max(1, totalBars) * kStepsPerBar64;
    const int clampedStep = std::clamp(step64, 0, maxStep - 1);
    auto& notes = slotNotes_[static_cast<size_t>(safeSlot)];

    const auto existing = std::ranges::find_if(notes, [&](const PianoRollNote& n) {
        return n.step64 == clampedStep;
    });
    if (existing != notes.end()) {
        if (existing->semitoneOffset == semitoneOffset) {
            notes.erase(existing);
            return false;
        }
        existing->semitoneOffset = semitoneOffset;
        existing->length64 = std::max(1, length64);
        return true;
    }

    addNote(notes, clampedStep, std::max(1, length64), semitoneOffset, 1.0f, false);
    sortAndDeduplicate(notes);
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
    const int safeSlot = std::clamp(activeSlot, 0, kNumAutoSlots - 1);
    const double safeBpm = std::max(20.0, bpm);
    const double safeRate = std::max(8000.0, sampleRate);
    const double safeBeatsPerBar = std::max(1.0, beatsPerBar);
    const int bars = std::clamp(settings.bars, 1, 64);

    const double ppq = hasHostPpq ? std::max(0.0, samplePpq) : internalPpq_;
    internalPpq_ += safeBpm / (60.0 * safeRate);

    const double windowBeats = static_cast<double>(bars) * safeBeatsPerBar;
    const double wrappedCycles = ppq / windowBeats;
    const double phase = wrappedCycles - std::floor(wrappedCycles);
    windowPhase_ = static_cast<float>(phase);

    const int totalSteps64 = bars * kStepsPerBar64;
    const int currentStep = std::clamp(
        static_cast<int>(std::floor(phase * static_cast<double>(totalSteps64))),
        0,
        totalSteps64 - 1);

    if (currentStep == lastStep64_ && safeSlot == lastSlot_) {
        return {};
    }
    lastStep64_ = currentStep;
    lastSlot_ = safeSlot;

    const auto& notes = slotNotes_[static_cast<size_t>(safeSlot)];
    if (notes.empty()) {
        return {};
    }

    const int patternSpan64 = std::max(1, loadedBars_[static_cast<size_t>(safeSlot)]) * kStepsPerBar64;
    const int lookupStep = currentStep % patternSpan64;

    const auto match = std::ranges::find_if(notes, [&](const PianoRollNote& n) {
        return n.step64 == lookupStep;
    });
    if (match == notes.end()) {
        return {};
    }

    const auto nextIt = std::next(match);
    const int nextStep = nextIt != notes.end() ? nextIt->step64 : (patternSpan64 + notes.front().step64);
    const int spacingSteps64 = std::clamp(nextStep - match->step64, 1, kStepsPerBar64);
    const double msPerBeat = 60000.0 / safeBpm;
    const double stepSpacingMs = (static_cast<double>(spacingSteps64) / static_cast<double>(kStepsPerBeat64)) * msPerBeat;

    const float effectiveTailMs = computeAutoEffectiveTailMs(baseTailMs, settings.tailShorten, stepSpacingMs);
    const float macroPitch = settings.pitchRise * windowPhase_;
    const float totalSemitones = static_cast<float>(match->semitoneOffset) + macroPitch;

    return AutoTriggerEvent{
        true,
        0,
        match->velocity,
        totalSemitones,
        effectiveTailMs,
        match->reverseCue,
        static_cast<double>(spacingSteps64) / static_cast<double>(kStepsPerBeat64)
    };
}

} // namespace openedmkick::core
