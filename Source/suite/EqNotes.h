#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace suite::eq {

inline constexpr int kLowMinMidi = 21;    // A0
inline constexpr int kLowMaxMidi = 45;    // A2
inline constexpr int kMidMinMidi = 57;    // A3
inline constexpr int kMidMaxMidi = 81;    // A5
inline constexpr int kHighMinMidi = 93;   // A6
inline constexpr int kHighMaxMidi = 117;  // A8

inline constexpr const char* kLowTip =
    "Low shelf. The inner dial sets gain; the outer ring steps chromatic notes from A0 to A2 (double-click the Note pill to type, e.g. G#1).";
inline constexpr const char* kMidTip =
    "Mid bell. The inner dial sets gain; the outer ring steps chromatic notes from A3 to A5.";
inline constexpr const char* kHighTip =
    "High shelf. The inner dial sets gain; the outer ring steps chromatic notes from A6 to A8.";
inline constexpr const char* kKeyTrackingTip =
    "Scales pitch with the played MIDI note. 0 keeps the root pitch. 1 follows the note.";

inline float noteToHz(int midiNote) noexcept {
    const int clamped = std::clamp(midiNote, 0, 144);
    return 440.0f * std::exp2((static_cast<float>(clamped) - 69.0f) / 12.0f);
}

inline int nearestMidi(float hz, int minMidi, int maxMidi) noexcept {
    if (!std::isfinite(hz) || hz <= 0.0f) return minMidi;
    const int midi = juce::roundToInt(69.0f + 12.0f * std::log2(hz / 440.0f));
    return std::clamp(midi, minMidi, maxMidi);
}

inline juce::String formatNote(float hz, int minMidi, int maxMidi) {
    static constexpr std::array<const char*, 12> kNames{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int midi = nearestMidi(hz, minMidi, maxMidi);
    const int octave = midi / 12 - 1;
    return juce::String(kNames[static_cast<size_t>(midi % 12)]) + juce::String(octave);
}

inline int pitchClassFromLetter(juce::juce_wchar letter) noexcept {
    switch (juce::CharacterFunctions::toUpperCase(letter)) {
        case 'C': return 0;
        case 'D': return 2;
        case 'E': return 4;
        case 'F': return 5;
        case 'G': return 7;
        case 'A': return 9;
        case 'B': return 11;
        default: return -1;
    }
}

inline std::optional<int> parseNoteMidi(const juce::String& text) {
    const auto trimmed = text.trim();
    if (trimmed.length() < 2) return std::nullopt;
    const int basePc = pitchClassFromLetter(trimmed[0]);
    if (basePc < 0) return std::nullopt;
    int alter = 0;
    int index = 1;
    if (index < trimmed.length() && trimmed[index] == '#') {
        alter = 1;
        ++index;
    } else if (index < trimmed.length() && (trimmed[index] == 'b' || trimmed[index] == 'B')) {
        alter = -1;
        ++index;
    }
    while (index < trimmed.length() && trimmed[index] == ' ') ++index;
    if (index >= trimmed.length()) return std::nullopt;
    const juce::String octaveText = trimmed.substring(index);
    if (octaveText.isEmpty() || !octaveText.containsOnly("0123456789")) return std::nullopt;
    const int octave = octaveText.getIntValue();
    const int pitchClass = (basePc + alter + 120) % 12;
    return (octave + 1) * 12 + pitchClass;
}

inline std::pair<int, int> bandLimits(const juce::String& freqParamId) noexcept {
    if (freqParamId == "eq_low_freq") return { kLowMinMidi, kLowMaxMidi };
    if (freqParamId == "eq_mid_freq") return { kMidMinMidi, kMidMaxMidi };
    if (freqParamId == "eq_high_freq") return { kHighMinMidi, kHighMaxMidi };
    return { kLowMinMidi, kLowMaxMidi };
}

template <typename ParseHz>
float parseNoteOrHz(const juce::String& text, int minMidi, int maxMidi, ParseHz parseHz) {
    const auto trimmed = text.trim();
    if (trimmed.isEmpty()) return noteToHz(minMidi);
    if (const auto midi = parseNoteMidi(trimmed))
        return noteToHz(std::clamp(*midi, minMidi, maxMidi));
    return noteToHz(nearestMidi(parseHz(trimmed), minMidi, maxMidi));
}

} // namespace suite::eq
