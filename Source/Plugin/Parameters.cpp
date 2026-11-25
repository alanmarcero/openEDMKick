#include "Parameters.h"
#include "suite/EqNotes.h"
#include "Effects.h"
#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>

namespace openedmkick::params {
namespace {

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Formatter = std::function<juce::String(float, int)>;
using Parser = std::function<float(const juce::String&)>;
constexpr int kVersion = 1;

// Each parser reads the text that its formatter writes.
struct Format {
    Formatter toText;
    Parser fromText;
};

const Parser kParseNumber = [](const juce::String& text) { return text.getFloatValue(); };
const Parser kParsePercent = [](const juce::String& text) { return text.getFloatValue() / 100.0f; };
const Parser kParseHz = [](const juce::String& text) {
    return text.getFloatValue() * (text.containsIgnoreCase("k") ? 1000.0f : 1.0f);
};

const Format kFormatHz{ [](float hz, int) { return formatHz(hz); }, kParseHz };
const Format kFormatOctaves{ [](float value, int) { return juce::String(value, 1) + " oct"; }, kParseNumber };
const Format kFormatNumber{ [](float value, int) { return juce::String(value, 2); }, kParseNumber };
const Format kFormatMidi{ [](float value, int) { return juce::String(juce::roundToInt(value)); }, kParseNumber };
// Key zones also accept note names. C2 is MIDI note 36.
const Format kFormatKey{ kFormatMidi.toText, [](const juce::String& text) {
    const auto trimmed = text.trim();
    if (trimmed.isEmpty()) return 0.0f;
    const int semitone = juce::String("C D EF G A B").indexOfIgnoreCase(trimmed.substring(0, 1));
    if (semitone < 0) return trimmed.getFloatValue();
    const auto afterLetter = trimmed.substring(1);
    const int sharp = afterLetter.startsWithChar('#') ? 1 : 0;
    const int flat = afterLetter.startsWithChar('b') ? 1 : 0;
    const int octave = afterLetter.substring(sharp + flat).getIntValue();
    return static_cast<float>((octave + 1) * 12 + semitone + sharp - flat);
} };
const Format kFormatPercent{ [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; }, kParsePercent };
const Format kFormatBipolar{ [](float value, int) {
    const int percent = juce::roundToInt(value * 100.0f);
    return (percent > 0 ? "+" : "") + juce::String(percent) + "%";
}, kParsePercent };
const Format kFormatPan{ [](float value, int) {
    const int percent = juce::roundToInt(value * 100.0f);
    if (percent == 0) return juce::String("C");
    return juce::String(std::abs(percent)) + (percent < 0 ? " L" : " R");
}, [](const juce::String& text) {
    const float amount = std::abs(text.getFloatValue()) / 100.0f;
    if (text.containsIgnoreCase("l")) return -amount;
    if (text.containsIgnoreCase("r")) return amount;
    return text.getFloatValue() / 100.0f;
} };
const Format kFormatMs{ [](float ms, int) {
    if (std::round(ms) >= 1000.0f) return juce::String(ms / 1000.0f, 2) + " s";
    if (std::round(ms * 10.0f) >= 100.0f) return juce::String(juce::roundToInt(ms)) + " ms";
    return juce::String(ms, 1) + " ms";
}, [](const juce::String& text) {
    const auto unit = text.trim();
    const bool seconds = unit.endsWithIgnoreCase("s") && !unit.endsWithIgnoreCase("ms");
    return text.getFloatValue() * (seconds ? 1000.0f : 1.0f);
} };
const Format kFormatDb{ [](float db, int) {
    const float tenths = std::round(db * 10.0f) / 10.0f;
    if (tenths == 0.0f) return juce::String("0.0 dB");
    return (tenths > 0.0f ? "+" : "") + juce::String(tenths, 1) + " dB";
}, kParseNumber };
const Format kFormatSemitones{ [](float semitones, int) {
    const int value = juce::roundToInt(semitones);
    return (value > 0 ? "+" : "") + juce::String(value) + " st";
}, kParseNumber };
const Format kFormatBpm{ [](float bpm, int) { return juce::String(bpm, 1) + " BPM"; }, kParseNumber };
const Format kFormatHighPass{ [](float hz, int) {
    if (hz <= 20.5f) return juce::String("Off");
    return formatHz(hz);
}, [](const juce::String& text) {
    if (text.trim().equalsIgnoreCase("off")) return core::ToneFilter::kHighPassBypassHz;
    return kParseHz(text);
} };
const Format kFormatLowPass{ [](float hz, int) {
    if (hz >= 19900.0f) return juce::String("Off");
    return formatHz(hz);
}, [](const juce::String& text) {
    if (text.trim().equalsIgnoreCase("off")) return core::ToneFilter::kLowPassBypassHz;
    return kParseHz(text);
} };

juce::NormalisableRange<float> skewedRange(float min, float max, float centre) {
    juce::NormalisableRange<float> range(min, max);
    range.setSkewForCentre(centre);
    return range;
}

juce::NormalisableRange<float> noteSnappedRange(int minMidi, int maxMidi) {
    const float minHz = eqNoteToHz(minMidi);
    const float maxHz = eqNoteToHz(maxMidi);
    return {
        minHz,
        maxHz,
        [minMidi, maxMidi](float, float, float proportion) {
            const float clamped = std::clamp(proportion, 0.0f, 1.0f);
            const int midi = juce::roundToInt(static_cast<float>(minMidi)
                + clamped * static_cast<float>(maxMidi - minMidi));
            return eqNoteToHz(std::clamp(midi, minMidi, maxMidi));
        },
        [minMidi, maxMidi](float, float, float value) {
            const int midi = eqHzToNearestMidi(value, minMidi, maxMidi);
            return static_cast<float>(midi - minMidi) / static_cast<float>(maxMidi - minMidi);
        },
        [minMidi, maxMidi](float, float, float value) {
            return eqNoteToHz(eqHzToNearestMidi(value, minMidi, maxMidi));
        }
    };
}

Format noteFormat(int minMidi, int maxMidi) {
    return {
        [minMidi, maxMidi](float hz, int) { return formatEqNote(hz, minMidi, maxMidi); },
        [minMidi, maxMidi](const juce::String& text) { return parseEqNoteOrHz(text, minMidi, maxMidi); }
    };
}

void addFloat(Layout& layout, const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float defaultValue, const Format& format,
              int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, version }, name, range, defaultValue,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(format.toText)
                                             .withValueFromStringFunction(format.fromText)));
}

void addChoice(Layout& layout, const juce::String& id, const juce::String& name,
               const juce::StringArray& choices, int defaultIndex, int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ id, version }, name, choices, defaultIndex));
}

void addBool(Layout& layout, const juce::String& id, const juce::String& name, bool defaultValue, int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ id, version }, name, defaultValue));
}

} // namespace

double beatsPerBarForSignature(int index) noexcept {
    const auto safeIndex = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kTimeSignatures.size()) - 1));
    const auto [numerator, denominator] = kTimeSignatures[safeIndex];
    return static_cast<double>(numerator) * (4.0 / static_cast<double>(denominator));
}

juce::String formatHz(float hz) {
    if (!std::isfinite(hz)) return "0 Hz";
    if (std::round(hz) >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
}

float eqNoteToHz(int midiNote) noexcept { return ::suite::eq::noteToHz(midiNote); }

int eqHzToNearestMidi(float hz, int minMidi, int maxMidi) noexcept {
    return ::suite::eq::nearestMidi(hz, minMidi, maxMidi);
}

juce::String formatEqNote(float hz, int minMidi, int maxMidi) {
    return ::suite::eq::formatNote(hz, minMidi, maxMidi);
}

std::pair<int, int> eqBandMidiLimits(const juce::String& freqParamId) noexcept {
    return ::suite::eq::bandLimits(freqParamId);
}

float parseEqNoteOrHz(const juce::String& text, int minMidi, int maxMidi) {
    return ::suite::eq::parseNoteOrHz(text, minMidi, maxMidi, [](const juce::String& trimmed) {
        return kParseHz(trimmed);
    });
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    Layout layout;
    const juce::StringArray syncDivisions{ "1/4", "1/8", "1/16", "1/32", "1/64" };
    const juce::StringArray reverseModes{ "Off", "Beat End (Sync)", "Beat 4 Auto", "1/2 Beat", "1/4 Beat" };

    addFloat(layout, kSweepOctaves, "Dive Range", { 0.0f, 6.0f }, 2.4f, kFormatOctaves);
    addFloat(layout, kSweepMs, "Dive Time", skewedRange(5.0f, 400.0f, 70.0f), 70.0f, kFormatMs);
    addFloat(layout, kSweepShape, "Dive Curve", { -1.0f, 1.0f }, 0.55f, kFormatBipolar);
    addFloat(layout, kKeyTracking, "Key Tracking", { 0.0f, 1.0f, 0.01f }, 1.0f, kFormatPercent);
    addFloat(layout, kSnapOctaves, "Snap Range", { 0.0f, 5.0f }, 2.0f, kFormatOctaves);
    addFloat(layout, kSnapMs, "Snap Time", skewedRange(0.2f, 12.0f, 2.0f), 2.0f, kFormatMs);
    addFloat(layout, kBodyTone, "Harmonics", { 0.0f, 1.0f }, 0.65f, kFormatPercent);
    addFloat(layout, kKnockHz, "Knock Freq", skewedRange(60.0f, 800.0f, 180.0f), 180.0f, kFormatHz);
    addFloat(layout, kKnockDb, "Knock", { 0.0f, 18.0f }, 6.0f, kFormatDb);
    addFloat(layout, kToneHz, "Body Tone", skewedRange(300.0f, 20000.0f, 9000.0f), 9000.0f, kFormatHz);
    addFloat(layout, kSubLevel, "Sub Level", { 0.0f, 1.0f }, 0.35f, kFormatPercent);
    addFloat(layout, kStartPhase, "Start Phase", { 0.0f, 1.0f }, 0.5f, kFormatPercent);
    addChoice(layout, kClickType, "Transient Source", { "Beater", "Noise", "Tick", "Analog" }, 0);
    addFloat(layout, kClickLevel, "Transient Level", { 0.0f, 1.0f }, 0.45f, kFormatPercent);
    addFloat(layout, kClickHz, "Transient Tone", skewedRange(500.0f, 14000.0f, 3500.0f), 3500.0f, kFormatHz);
    addFloat(layout, kClickMs, "Transient Length", skewedRange(0.5f, 40.0f, 6.0f), 6.0f, kFormatMs);
    addFloat(layout, kAttackMs, "Attack", { 0.05f, 10.0f }, 0.15f, kFormatMs);
    addFloat(layout, kHoldMs, "Hold", { 0.0f, 400.0f }, 45.0f, kFormatMs);
    addFloat(layout, kDecayMs, "Decay", skewedRange(1.0f, 1500.0f, 300.0f), 300.0f, kFormatMs);
    addFloat(layout, kDecayShape, "Decay Curve", { -1.0f, 1.0f }, 0.5f, kFormatBipolar);
    addFloat(layout, kLowCutHz, "Low Cut", skewedRange(10.0f, 90.0f, 25.0f), 25.0f, kFormatHz);

    addBool(layout, kClockSync, "Host Sync", true);
    addFloat(layout, kTempoBpm, "Tempo", { 40.0f, 240.0f }, 128.0f, kFormatBpm);
    addChoice(layout, kTimeSignature, "Time Signature",
              { "4/4", "3/4", "2/4", "5/4", "6/4", "6/8", "7/8", "9/8", "12/8" }, 0);

    addChoice(layout, kReverseMode, "Reverse Sync Mode", reverseModes, 0);
    addFloat(layout, kReverseMix, "Reverse Mix", { 0.0f, 1.0f }, 1.0f, kFormatPercent);
    addChoice(layout, kDistType, "Distortion Type", { "Tape", "Tube", "Hard Clip", "Wavefold" }, 0);
    addFloat(layout, kDistDrive, "Distortion Drive", { 0.0f, 1.0f, 0.01f }, 0.0f, kFormatPercent);
    addFloat(layout, kDistTone, "Distortion Tone", { -1.0f, 1.0f, 0.01f }, 0.0f, kFormatBipolar);
    addFloat(layout, kDistMix, "Distortion Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, kFormatPercent);
    addFloat(layout, kCompThresh, "Comp Threshold", { -40.0f, 0.0f, 0.5f }, -14.0f, kFormatDb);
    addFloat(layout, kCompRatio, "Comp Ratio", { 1.0f, 20.0f, 0.1f }, 4.0f, kFormatNumber);
    addFloat(layout, kCompAttack, "Comp Attack", skewedRange(0.1f, 50.0f, 5.0f), 5.0f, kFormatMs);
    addFloat(layout, kCompRelease, "Comp Release", skewedRange(10.0f, 500.0f, 80.0f), 80.0f, kFormatMs);
    addFloat(layout, kCompMakeup, "Comp Makeup", { 0.0f, 24.0f, 0.5f }, 0.0f, kFormatDb);
    addFloat(layout, kCompMix, "Comp Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, kFormatPercent);
    addFloat(layout, kReverbSize, "Reverb Size", { 0.1f, 1.0f, 0.01f }, 0.65f, kFormatPercent);
    addFloat(layout, kReverbDecay, "Reverb Decay", skewedRange(50.0f, 2500.0f, 680.0f), 680.0f, kFormatMs);
    addFloat(layout, kReverbDamp, "Reverb Damp", { 0.0f, 1.0f, 0.01f }, 0.4f, kFormatPercent);
    addFloat(layout, kReverbGate, "Reverb Gate", { 0.0f, 1400.0f, 5.0f }, 0.0f, kFormatMs);
    addFloat(layout, kReverbMix, "Reverb Mix", { 0.0f, 1.0f, 0.01f }, 0.0f, kFormatPercent);
    addFloat(layout, kHighPass, "High Pass", skewedRange(20.0f, 8000.0f, 250.0f), 20.0f, kFormatHighPass);
    addFloat(layout, kLowPass, "Low Pass", skewedRange(40.0f, 20000.0f, 2500.0f), 20000.0f, kFormatLowPass);
    addBool(layout, kHighPassSyncEnabled, "High Pass Sync", false);
    addChoice(layout, kHighPassSyncRate, "High-pass Rate", syncDivisions, 2);
    addBool(layout, kLowPassSyncEnabled, "Low Pass Sync", false);
    addChoice(layout, kLowPassSyncRate, "Low-pass Rate", syncDivisions, 2);
    addFloat(layout, kOutput, "Output", { -24.0f, 12.0f }, 0.0f, kFormatDb);
    addBool(layout, kLimiterEnabled, "Limiter", true);
    addFloat(layout, kCeiling, "Ceiling", { -18.0f, 0.0f }, -0.3f, kFormatDb);
    addBool(layout, kAmpEnabled, "Hold MIDI Notes", false, 4);
    addFloat(layout, kAmpAttack, "Envelope Attack", { 0.0f, 10000.0f }, 0.0f, kFormatMs, 4);
    addFloat(layout, kAmpDecay, "Envelope Decay", { 0.0f, 10000.0f }, 100.0f, kFormatMs, 4);
    addFloat(layout, kAmpSustain, "Envelope Sustain", { 0.0f, 1.0f }, 0.8f, kFormatPercent, 4);
    addFloat(layout, kAmpRelease, "Envelope Release", { 0.0f, 20000.0f }, 300.0f, kFormatMs, 4);
    addBool(layout, kReverseEnabled, "Reverse MIDI", false, 4);
    addFloat(layout, kMidiVolume, "MIDI Volume", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addFloat(layout, kMidiExpression, "MIDI Expression", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addFloat(layout, kMidiBrightness, "MIDI Brightness", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addBool(layout, kHighPassMidi, "High-pass Sweep", false, 4);
    addBool(layout, kLowPassMidi, "Low-pass Sweep", false, 4);
    addFloat(layout, kSubOctave, "Sub Octave", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kClickPunch, "Transient Punch", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kStereoWidth, "Top Width", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kEqLowFreq, "Low EQ Freq", noteSnappedRange(kEqLowMinMidi, kEqLowMaxMidi), eqNoteToHz(40), noteFormat(kEqLowMinMidi, kEqLowMaxMidi), 6);
    addFloat(layout, kEqLowDb, "Low EQ Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    addFloat(layout, kEqMidFreq, "Mid EQ Freq", noteSnappedRange(kEqMidMinMidi, kEqMidMaxMidi), eqNoteToHz(83), noteFormat(kEqMidMinMidi, kEqMidMaxMidi), 6);
    addFloat(layout, kEqMidDb, "Mid EQ Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    addFloat(layout, kEqHighFreq, "High EQ Freq", noteSnappedRange(kEqHighMinMidi, kEqHighMaxMidi), eqNoteToHz(118), noteFormat(kEqHighMinMidi, kEqHighMaxMidi), 6);
    addFloat(layout, kEqHighDb, "High EQ Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    return layout;
}

} // namespace openedmkick::params
