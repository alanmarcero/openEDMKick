#include "Parameters.h"
#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace openedmkick::params {
namespace {

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Formatter = std::function<juce::String(float, int)>;
constexpr int kVersion = 1;

const Formatter kFormatHz = [](float hz, int) { return formatHz(hz); };
const Formatter kFormatOctaves = [](float value, int) { return juce::String(value, 1) + " oct"; };
const Formatter kFormatNumber = [](float value, int) { return juce::String(value, 2); };
const Formatter kFormatMidi = [](float value, int) { return juce::String(juce::roundToInt(value)); };
const Formatter kFormatPercent = [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; };
const Formatter kFormatBipolar = [](float value, int) {
    const int percent = juce::roundToInt(value * 100.0f);
    return (percent > 0 ? "+" : "") + juce::String(percent) + "%";
};
const Formatter kFormatMs = [](float ms, int) {
    return (ms < 10.0f ? juce::String(ms, 1) : juce::String(juce::roundToInt(ms))) + " ms";
};
const Formatter kFormatDb = [](float db, int) { return (db > 0.05f ? "+" : "") + juce::String(db, 1) + " dB"; };
const Formatter kFormatSemitones = [](float semitones, int) {
    const int value = juce::roundToInt(semitones);
    return (value > 0 ? "+" : "") + juce::String(value) + " st";
};
const Formatter kFormatBpm = [](float bpm, int) { return juce::String(bpm, 1) + " BPM"; };
const Formatter kFormatHighPass = [](float hz, int) { return hz <= 20.5f ? juce::String("Off") : formatHz(hz); };
const Formatter kFormatLowPass = [](float hz, int) { return hz >= 19900.0f ? juce::String("Off") : formatHz(hz); };

juce::NormalisableRange<float> skewedRange(float min, float max, float centre) {
    juce::NormalisableRange<float> range(min, max);
    range.setSkewForCentre(centre);
    return range;
}

void addFloat(Layout& layout, const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float defaultValue, const Formatter& formatter,
              int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, version }, name, range, defaultValue,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(formatter)));
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

juce::String autoSlotId(int slot, const juce::String& baseId) {
    if (slot <= 0) return baseId;
    const auto slotChar = static_cast<juce::juce_wchar>('a' + std::clamp(slot, 0, 3));
    return "auto_" + juce::String::charToString(slotChar) + "_" + baseId.substring(5);
}

juce::String layerId(int layer, const juce::String& baseId) {
    const auto slotChar = static_cast<juce::juce_wchar>('a' + std::clamp(layer, 0, 3));
    return "layer_" + juce::String::charToString(slotChar) + "_" + baseId;
}

double beatsPerBarForSignature(int index) noexcept {
    const auto safeIndex = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kTimeSignatures.size()) - 1));
    const auto [numerator, denominator] = kTimeSignatures[safeIndex];
    return static_cast<double>(numerator) * (4.0 / static_cast<double>(denominator));
}

juce::String formatHz(float hz) {
    if (!std::isfinite(hz)) return "0 Hz";
    if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 2) + " kHz";
    return (hz < 100.0f ? juce::String(hz, 1) : juce::String(juce::roundToInt(hz))) + " Hz";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    Layout layout;
    const juce::StringArray barChoices{ "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars", "32 Bars", "64 Bars" };
    const juce::StringArray gridChoices{ "1/4", "1/8", "1/16", "1/32", "1/64" };
    const juce::StringArray syncDivisions{ "1/4", "1/8", "1/16", "1/32", "1/64" };
    const juce::StringArray reverseModes{ "Off", "Beat End (Sync)", "Beat 4 Auto", "1/2 Beat", "1/4 Beat" };
    juce::StringArray fillChoices;
    std::ranges::for_each(core::kFillPresetNames, [&](std::string_view name) {
        fillChoices.add(juce::String(name.data(), name.size()));
    });

    addFloat(layout, kSweepOctaves, "Sweep", { 0.0f, 6.0f }, 2.4f, kFormatOctaves);
    addFloat(layout, kSweepMs, "Sweep Time", skewedRange(5.0f, 400.0f, 70.0f), 70.0f, kFormatMs);
    addFloat(layout, kSweepShape, "Sweep Shape", { -1.0f, 1.0f }, 0.55f, kFormatBipolar);
    addFloat(layout, kSnapOctaves, "Snap", { 0.0f, 5.0f }, 2.0f, kFormatOctaves);
    addFloat(layout, kSnapMs, "Snap Time", skewedRange(0.2f, 12.0f, 2.0f), 2.0f, kFormatMs);
    addFloat(layout, kBodyTone, "Harmonics", { 0.0f, 1.0f }, 0.65f, kFormatPercent);
    addFloat(layout, kKnockHz, "Knock Frequency", skewedRange(60.0f, 800.0f, 180.0f), 180.0f, kFormatHz);
    addFloat(layout, kKnockDb, "Knock", { 0.0f, 18.0f }, 6.0f, kFormatDb);
    addFloat(layout, kToneHz, "Body Tone", skewedRange(300.0f, 20000.0f, 9000.0f), 9000.0f, kFormatHz);
    addFloat(layout, kSubLevel, "Sub", { 0.0f, 1.0f }, 0.35f, kFormatPercent);
    addFloat(layout, kStartPhase, "Start Phase", { 0.0f, 1.0f }, 0.5f, kFormatPercent);
    addChoice(layout, kClickType, "Transient Type", { "Beater", "Noise", "Tick", "Analog" }, 0);
    addFloat(layout, kClickLevel, "Transient", { 0.0f, 1.0f }, 0.45f, kFormatPercent);
    addFloat(layout, kClickHz, "Transient Frequency", skewedRange(500.0f, 14000.0f, 3500.0f), 3500.0f, kFormatHz);
    addFloat(layout, kClickMs, "Transient Length", skewedRange(0.5f, 40.0f, 6.0f), 6.0f, kFormatMs);
    addFloat(layout, kAttackMs, "Attack", { 0.05f, 10.0f }, 0.15f, kFormatMs);
    addFloat(layout, kHoldMs, "Hold", { 0.0f, 400.0f }, 45.0f, kFormatMs);
    addFloat(layout, kDecayMs, "Decay", skewedRange(1.0f, 1500.0f, 300.0f), 300.0f, kFormatMs);
    addFloat(layout, kDecayShape, "Decay Shape", { -1.0f, 1.0f }, 0.5f, kFormatBipolar);
    addFloat(layout, kLowCutHz, "Low Cut", skewedRange(10.0f, 90.0f, 25.0f), 25.0f, kFormatHz);

    const std::array<float, 4> layerEnabled{ 1.0f, 0.0f, 0.0f, 0.0f };
    const std::array<float, 4> layerSound{ 0.0f, 1.0f, 2.0f, 3.0f };
    const std::array<float, 4> layerGain{ 1.0f, 0.65f, 0.5f, 0.35f };
    const std::array<float, 4> layerTranspose{ 0.0f, 12.0f, 0.0f, 0.0f };
    const std::array<float, 4> layerDecay{ 300.0f, 220.0f, 140.0f, 90.0f };
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int layerIndex) {
        const auto id = [layerIndex](const char* key) { return layerId(layerIndex, key); };
        const auto suffix = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layerIndex));
        addBool(layout, id(kLayerEnabled), "Layer " + suffix + " On", layerEnabled[static_cast<size_t>(layerIndex)] > 0.5f);
        addChoice(layout, id(kLayerSound), "Layer " + suffix + " Sound", { "Kick", "Snare", "Clap", "Hat" },
                  static_cast<int>(layerSound[static_cast<size_t>(layerIndex)]));
        addFloat(layout, id(kLayerGain), "Layer " + suffix + " Level", { 0.0f, 1.0f }, layerGain[static_cast<size_t>(layerIndex)], kFormatPercent);
        addFloat(layout, id(kLayerTranspose), "Layer " + suffix + " Pitch", { -24.0f, 24.0f, 1.0f },
                 layerTranspose[static_cast<size_t>(layerIndex)], kFormatSemitones);
        addFloat(layout, id(kLayerDecay), "Layer " + suffix + " Decay", skewedRange(5.0f, 1500.0f, 220.0f),
                 layerDecay[static_cast<size_t>(layerIndex)], kFormatMs);
        addFloat(layout, id(kLayerHighPass), "Layer " + suffix + " High Pass", skewedRange(20.0f, 8000.0f, 250.0f),
                 20.0f, kFormatHighPass);
        addFloat(layout, id(kLayerLowPass), "Layer " + suffix + " Low Pass", skewedRange(40.0f, 20000.0f, 2500.0f),
                 20000.0f, kFormatLowPass);
    });

    addBool(layout, kClockSync, "Host Sync", true);
    addFloat(layout, kTempoBpm, "Tempo", { 40.0f, 240.0f }, 128.0f, kFormatBpm);
    addChoice(layout, kTimeSignature, "Time Signature",
              { "4/4", "3/4", "2/4", "5/4", "6/4", "6/8", "7/8", "9/8", "12/8" }, 0);
    addBool(layout, kAutoEnabled, "Auto Mode On", false);
    addBool(layout, kAutoBypass, "Auto Bypass", false);
    addChoice(layout, kAutoSlot, "Auto Mode", { "A", "B", "C", "D" }, 0);
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const auto id = [slot](const char* key) { return autoSlotId(slot, key); };
        const auto prefix = "Auto " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)) + " ";
        const int version = slot == 0 ? 1 : 2;
        addChoice(layout, id(kAutoBars), prefix + "Bars", barChoices, 2, version);
        addChoice(layout, id(kAutoFillPreset), prefix + "Fill Preset", fillChoices, slot % core::kNumFillPresets, version);
        addChoice(layout, id(kAutoGrid), prefix + "Grid", gridChoices, 4, version);
        addFloat(layout, id(kAutoTailShorten), prefix + "Tail Shortener", { 0.0f, 1.0f }, 0.65f, kFormatPercent, version);
        addFloat(layout, id(kAutoCrushSweep), prefix + "Crush Ramp", { 0.0f, 1.0f }, 0.0f, kFormatPercent, version);
    });

    addChoice(layout, kReverseMode, "Reverse Sync Mode", reverseModes, 0);
    addFloat(layout, kReverseMix, "Reverse Mix", { 0.0f, 1.0f }, 1.0f, kFormatPercent);
    addFloat(layout, kCrushRate, "Crush Rate", skewedRange(200.0f, 48000.0f, 4000.0f), 8000.0f, kFormatHz);
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ kCrushBits, kVersion }, "Crush Bits", 1, 16, 8));
    addFloat(layout, kCrushMix, "Crush Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kStutterRate, "Stutter Rate", syncDivisions, 2);
    addFloat(layout, kStutterMix, "Stutter Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kGateRate, "Gate Rate", syncDivisions, 2);
    addFloat(layout, kGateMix, "Gate Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kPumpRate, "Pump Rate", syncDivisions, 0);
    addFloat(layout, kPumpMix, "Pump Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addFloat(layout, kHighPass, "High Pass", skewedRange(20.0f, 8000.0f, 250.0f), 20.0f, kFormatHighPass);
    addFloat(layout, kLowPass, "Low Pass", skewedRange(40.0f, 20000.0f, 2500.0f), 20000.0f, kFormatLowPass);
    addBool(layout, kHighPassSyncEnabled, "High Pass Sync", false);
    addChoice(layout, kHighPassSyncRate, "High Pass Sync Rate", syncDivisions, 2);
    addBool(layout, kLowPassSyncEnabled, "Low Pass Sync", false);
    addChoice(layout, kLowPassSyncRate, "Low Pass Sync Rate", syncDivisions, 2);
    addFloat(layout, kOutput, "Output", { -24.0f, 12.0f }, 0.0f, kFormatDb);
    addBool(layout, kLimiter, "Limiter", true);
    addFloat(layout, kCeiling, "Ceiling", { -18.0f, 0.0f }, -0.3f, kFormatDb);
    addBool(layout, kMasterBypass, "Master Bypass", false);
    // Append controls to preserve the host's existing parameter order.
    for (int layer = 0; layer < core::kNumSoundLayers; ++layer) {
        const auto prefix = "Layer " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer)) + " ";
        addFloat(layout, layerId(layer, kLayerVelocityCurve), prefix + "Velocity curve", { 0.1f, 4.0f }, 1.45f, kFormatNumber, 3);
        addFloat(layout, layerId(layer, kLayerVelocityTimbre), prefix + "Velocity timbre", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 3);
        addFloat(layout, layerId(layer, kLayerKeyLow), prefix + "Key low", { 0.0f, 127.0f, 1.0f }, 0.0f, kFormatMidi, 3);
        addFloat(layout, layerId(layer, kLayerKeyHigh), prefix + "Key high", { 0.0f, 127.0f, 1.0f }, 127.0f, kFormatMidi, 3);
        addFloat(layout, layerId(layer, kLayerVelocityLow), prefix + "Velocity low", { 1.0f, 127.0f, 1.0f }, 1.0f, kFormatMidi, 3);
        addFloat(layout, layerId(layer, kLayerVelocityHigh), prefix + "Velocity high", { 1.0f, 127.0f, 1.0f }, 127.0f, kFormatMidi, 3);
        addFloat(layout, layerId(layer, kLayerToneScale), prefix + "Tone scale", { 0.1f, 2.0f }, 1.0f, kFormatPercent, 3);
        addFloat(layout, layerId(layer, kLayerSweepScale), prefix + "Sweep scale", { 0.0f, 2.0f }, 1.0f, kFormatPercent, 3);
        addBool(layout, layerId(layer, kLayerSolo), prefix + "Solo", false, 3);
        addFloat(layout, layerId(layer, kLayerPan), prefix + "Pan", { -1.0f, 1.0f }, 0.0f, kFormatPercent, 3);
        addBool(layout, layerId(layer, kLayerPolarity), prefix + "Invert", false, 3);
    }
    addBool(layout, kAmpEnabled, "Amp Enabled", false, 4);
    addFloat(layout, kAmpAttack, "Amp Attack Ms", { 0.0f, 10000.0f }, 0.0f, kFormatMs, 4);
    addFloat(layout, kAmpDecay, "Amp Decay Ms", { 0.0f, 10000.0f }, 100.0f, kFormatMs, 4);
    addFloat(layout, kAmpSustain, "Amp Sustain", { 0.0f, 1.0f }, 0.8f, kFormatPercent, 4);
    addFloat(layout, kAmpRelease, "Amp Release Ms", { 0.0f, 20000.0f }, 300.0f, kFormatMs, 4);
    addBool(layout, kReverseEnabled, "Reverse On", false, 4);
    addBool(layout, kCrushEnabled, "Crush On", false, 4);
    addBool(layout, kStutterEnabled, "Stutter On", false, 4);
    addBool(layout, kGateEnabled, "Gate On", false, 4);
    addFloat(layout, kMidiVolume, "Midi Volume", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addFloat(layout, kMidiExpression, "Midi Expression", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addFloat(layout, kMidiBrightness, "Midi Brightness", { 0.0f, 1.0f }, 1.0f, kFormatPercent, 4);
    addBool(layout, kHighPassMidi, "HP MIDI activation", false, 4);
    addBool(layout, kLowPassMidi, "LP MIDI activation", false, 4);
    addFloat(layout, kSubOctave, "Sub Octave", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kClickPunch, "Transient Punch", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kStereoWidth, "Stereo Width", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 5);
    addFloat(layout, kEqLowFreq, "EQ Low Frequency", skewedRange(25.0f, 400.0f, 90.0f), 80.0f, kFormatHz, 6);
    addFloat(layout, kEqLowDb, "EQ Low Level", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    addFloat(layout, kEqMidFreq, "EQ Mid Frequency", skewedRange(200.0f, 6000.0f, 1000.0f), 1000.0f, kFormatHz, 6);
    addFloat(layout, kEqMidDb, "EQ Mid Level", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    addFloat(layout, kEqHighFreq, "EQ High Frequency", skewedRange(2000.0f, 18000.0f, 6000.0f), 7500.0f, kFormatHz, 6);
    addFloat(layout, kEqHighDb, "EQ High Level", { -12.0f, 12.0f, 0.1f }, 0.0f, kFormatDb, 6);
    // Appended so older hosts keep the existing parameter order.
    addBool(layout, kClickSampleOn, "Sample Click", false, 7);
    addChoice(layout, kClickSampleSource, "Sample Click Source", { "Tick", "Beater", "Wood", "Snap", "User" }, 0, 7);
    addFloat(layout, kClickSampleLevel, "Sample Click Level", { 0.0f, 1.0f }, 0.75f, kFormatPercent, 7);
    addFloat(layout, kClickSampleStart, "Sample Click Start", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 7);
    addFloat(layout, kClickSamplePitch, "Sample Click Pitch", { -24.0f, 24.0f, 0.01f }, 0.0f, kFormatSemitones, 7);
    addFloat(layout, kClickSampleHp, "Sample Click High Pass", skewedRange(20.0f, 8000.0f, 800.0f), 20.0f, kFormatHighPass, 7);
    return layout;
}

} // namespace openedmkick::params
