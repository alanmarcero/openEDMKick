#include "Parameters.h"
#include <algorithm>
#include <ranges>

namespace openedmkick::params {

namespace {

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Attributes = juce::AudioParameterFloatAttributes;
using Formatter = std::function<juce::String(float, int)>;

constexpr int kVersion = 1;

const Formatter kFormatHz = [](float hz, int) { return formatHz(hz); };
const Formatter kFormatPercent = [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; };
const Formatter kFormatBipolar = [](float value, int) {
    const int percent = juce::roundToInt(value * 100.0f);
    return (percent > 0 ? "+" : "") + juce::String(percent) + "%";
};
const Formatter kFormatMs = [](float ms, int) {
    return (ms < 10.0f ? juce::String(ms, 1) : juce::String(juce::roundToInt(ms))) + " ms";
};
const Formatter kFormatDb = [](float db, int) { return juce::String(db, 1) + " dB"; };
const Formatter kFormatBpm = [](float bpm, int) { return juce::String(bpm, 1) + " BPM"; };
const Formatter kFormatCents = [](float cents, int) {
    const int rounded = juce::roundToInt(cents);
    return (rounded > 0 ? "+" : "") + juce::String(rounded) + " ct";
};
const Formatter kFormatSemitones = [](float st, int) {
    return "+" + juce::String(st, 1) + " st";
};

juce::NormalisableRange<float> skewedRange(float min, float max, float centre) {
    juce::NormalisableRange<float> range(min, max);
    range.setSkewForCentre(centre);
    return range;
}

void addFloat(
    Layout& layout,
    const juce::String& id,
    const juce::String& name,
    juce::NormalisableRange<float> range,
    float defaultValue,
    const Formatter& formatter,
    int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, version },
        name,
        range,
        defaultValue,
        Attributes().withStringFromValueFunction(formatter)));
}

void addChoice(
    Layout& layout,
    const juce::String& id,
    const juce::String& name,
    const juce::StringArray& choices,
    int defaultIndex,
    int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ id, version },
        name,
        choices,
        defaultIndex));
}

void addBool(Layout& layout, const juce::String& id, const juce::String& name, bool defaultValue) {
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ id, kVersion },
        name,
        defaultValue));
}

} // namespace

juce::String autoSlotId(int slot, const juce::String& baseId) {
    if (slot <= 0) {
        return baseId;
    }
    const auto slotChar = static_cast<juce::juce_wchar>('a' + std::clamp(slot, 0, 3));
    return "auto_" + juce::String::charToString(slotChar) + "_" + baseId.substring(5);
}

double beatsPerBarForSignature(int index) noexcept {
    const auto safeIndex = static_cast<size_t>(
        std::clamp(index, 0, static_cast<int>(kTimeSignatures.size()) - 1));
    const auto [numerator, denominator] = kTimeSignatures[safeIndex];
    return static_cast<double>(numerator) * (4.0 / static_cast<double>(denominator));
}

juce::String formatHz(float hz) {
    if (hz >= 1000.0f) {
        return juce::String(hz / 1000.0f, 2) + " kHz";
    }
    return (hz < 100.0f ? juce::String(hz, 1) : juce::String(juce::roundToInt(hz))) + " Hz";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    Layout layout;

    const juce::StringArray notes{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const juce::StringArray octaves{ "0 Sub", "1 Club", "2 Punch" };
    const juce::StringArray barChoices{
        "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars", "32 Bars", "64 Bars"
    };
    const juce::StringArray gridChoices{ "1/4", "1/8", "1/16", "1/32", "1/64" };
    const juce::StringArray syncDivisions{ "1/4", "1/8", "1/16", "1/32", "1/64" };
    const juce::StringArray reverseModes{
        "Off", "Beat End (Sync)", "Beat 4 Auto", "1/2 Beat", "1/4 Beat"
    };

    juce::StringArray fillChoices;
    std::ranges::for_each(core::kFillPresetNames, [&](std::string_view name) {
        fillChoices.add(juce::String(name.data(), name.size()));
    });

    // Kick drum synthesis & 12-note tuning
    addChoice(layout, kNote, "Kick Note", notes, 5); // Default F
    addChoice(layout, kOctave, "Kick Octave", octaves, 1);
    addFloat(layout, kFineTune, "Fine Tune", { -50.0f, 50.0f }, 0.0f, kFormatCents);
    addFloat(layout, kStartPitch, "Start Pitch", skewedRange(100.0f, 2500.0f, 450.0f), 420.0f, kFormatHz);
    addFloat(layout, kPitchDecay, "Pitch Decay", skewedRange(5.0f, 250.0f, 45.0f), 42.0f, kFormatMs);
    addFloat(layout, kPitchCurve, "Pitch Curve", { -1.0f, 1.0f }, 0.25f, kFormatBipolar);
    addFloat(layout, kTailLength, "Tail Length", skewedRange(30.0f, 1200.0f, 320.0f), 380.0f, kFormatMs);
    addFloat(layout, kTailCurve, "Tail Curve", { -1.0f, 1.0f }, 0.15f, kFormatBipolar);
    addFloat(layout, kSubDrive, "Sub Drive", { 0.0f, 1.0f }, 0.25f, kFormatPercent);
    addFloat(layout, kClickLevel, "Click Level", { 0.0f, 1.0f }, 0.35f, kFormatPercent);
    addFloat(layout, kClickFreq, "Click Freq", skewedRange(500.0f, 10000.0f, 2500.0f), 2800.0f, kFormatHz);
    addFloat(layout, kPunch, "Punch", { 0.0f, 1.0f }, 0.45f, kFormatPercent);
    addBool(layout, kLockIn, "Lock In (Cached WAV)", false);

    // Transport & Clock Sync
    addBool(layout, kClockSync, "Host Sync", true);
    addFloat(layout, kTempoBpm, "Tempo", { 40.0f, 240.0f }, 128.0f, kFormatBpm);
    addChoice(layout, kTimeSignature, "Time Signature",
              { "4/4", "3/4", "2/4", "5/4", "6/4", "6/8", "7/8", "9/8", "12/8" }, 0);

    // A/B/C/D Auto Mode & 1/64 Piano Roll
    addBool(layout, kAutoEnabled, "Auto Mode On", false);
    addBool(layout, kAutoBypass, "Auto Bypass", false);
    addChoice(layout, kAutoSlot, "Auto Mode", { "A", "B", "C", "D" }, 0);

    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const auto id = [slot](const char* key) { return autoSlotId(slot, key); };
        const auto prefix = "Auto " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)) + " ";
        const int version = slot == 0 ? 1 : 2;
        addChoice(layout, id(kAutoBars), prefix + "Bars", barChoices, 2, version); // default 4 Bars
        addChoice(layout, id(kAutoFillPreset), prefix + "Fill Preset", fillChoices, slot % core::kNumFillPresets, version);
        addChoice(layout, id(kAutoGrid), prefix + "Grid", gridChoices, 4, version); // default 1/64
        addFloat(layout, id(kAutoTailShorten), prefix + "Tail Shortener", { 0.0f, 1.0f }, 0.65f, kFormatPercent, version);
        addFloat(layout, id(kAutoPitchRise), prefix + "Pitch Rise", { 0.0f, 12.0f }, 0.0f, kFormatSemitones, version);
        addFloat(layout, id(kAutoCrushSweep), prefix + "Crush Ramp", { 0.0f, 1.0f }, 0.0f, kFormatPercent, version);
    });

    // Host-Synced Effects & Output
    addChoice(layout, kReverseMode, "Reverse Sync Mode", reverseModes, 0);
    addFloat(layout, kReverseMix, "Reverse Mix", { 0.0f, 1.0f }, 1.0f, kFormatPercent);
    addFloat(layout, kDrive, "Drive", { 0.0f, 1.0f }, 0.15f, kFormatPercent);
    addFloat(layout, kCrushRate, "Crush Rate", skewedRange(200.0f, 48000.0f, 4000.0f), 8000.0f, kFormatHz);
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ kCrushBits, kVersion }, "Crush Bits", 1, 16, 8));
    addFloat(layout, kCrushMix, "Crush Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kStutterRate, "Stutter Rate", syncDivisions, 2);
    addFloat(layout, kStutterMix, "Stutter Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kGateRate, "Gate Rate", syncDivisions, 2);
    addFloat(layout, kGateMix, "Gate Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kPumpRate, "Pump Rate", syncDivisions, 0);
    addFloat(layout, kPumpMix, "Pump Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addFloat(layout, kOutput, "Output", { -24.0f, 12.0f }, 0.0f, kFormatDb);
    addBool(layout, kLimiter, "Limiter", true);
    addFloat(layout, kCeiling, "Ceiling", { -18.0f, 0.0f }, -0.3f, kFormatDb);
    addBool(layout, kMasterBypass, "Master Bypass", false);

    return layout;
}

} // namespace openedmkick::params
