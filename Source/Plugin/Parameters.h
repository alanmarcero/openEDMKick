#pragma once

#include "AutoSequencer.h"
#include <array>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>

namespace openedmkick::params {

inline constexpr const char* kSweepOctaves = "sweep_octaves";
inline constexpr const char* kSweepMs = "sweep_ms";
inline constexpr const char* kSweepShape = "sweep_shape";
inline constexpr const char* kSnapOctaves = "snap_octaves";
inline constexpr const char* kSnapMs = "snap_ms";
inline constexpr const char* kBodyTone = "body_tone";
inline constexpr const char* kBodyDrive = "body_drive";
inline constexpr const char* kSaturation = "saturation";
inline constexpr const char* kKnockHz = "knock_hz";
inline constexpr const char* kKnockDb = "knock_db";
inline constexpr const char* kGrit = "grit";
inline constexpr const char* kToneHz = "tone_hz";
inline constexpr const char* kSubLevel = "sub_level";
inline constexpr const char* kStartPhase = "start_phase";
inline constexpr const char* kClickType = "click_type";
inline constexpr const char* kClickLevel = "click_level";
inline constexpr const char* kClickHz = "click_hz";
inline constexpr const char* kClickMs = "click_ms";
inline constexpr const char* kAttackMs = "attack_ms";
inline constexpr const char* kHoldMs = "hold_ms";
inline constexpr const char* kDecayMs = "decay_ms";
inline constexpr const char* kDecayShape = "decay_shape";
inline constexpr const char* kLowCutHz = "low_cut_hz";
inline constexpr const char* kClip = "clip";

inline constexpr const char* kLayerEnabled = "enabled";
inline constexpr const char* kLayerSound = "sound";
inline constexpr const char* kLayerGain = "gain";
inline constexpr const char* kLayerTranspose = "transpose";
inline constexpr const char* kLayerDecay = "decay";
inline constexpr const char* kLayerHighPass = "hp";
inline constexpr const char* kLayerLowPass = "lp";

inline constexpr const char* kLayerVelocityCurve = "velocity_curve";
inline constexpr const char* kLayerVelocityTimbre = "velocity_timbre";
inline constexpr const char* kLayerKeyLow = "key_low";
inline constexpr const char* kLayerKeyHigh = "key_high";
inline constexpr const char* kLayerVelocityLow = "velocity_low";
inline constexpr const char* kLayerVelocityHigh = "velocity_high";
inline constexpr const char* kLayerDriveOffset = "drive_offset";
inline constexpr const char* kLayerToneScale = "tone_scale";
inline constexpr const char* kLayerSweepScale = "sweep_scale";
inline constexpr const char* kLayerSolo = "solo";
inline constexpr const char* kLayerPan = "pan";
inline constexpr const char* kLayerPolarity = "polarity";

inline constexpr const char* kAmpEnabled = "amp_enabled";
inline constexpr const char* kAmpAttack = "amp_attack_ms";
inline constexpr const char* kAmpDecay = "amp_decay_ms";
inline constexpr const char* kAmpSustain = "amp_sustain";
inline constexpr const char* kAmpRelease = "amp_release_ms";
inline constexpr const char* kReverseEnabled = "reverse_on";
inline constexpr const char* kCrushEnabled = "crush_on";
inline constexpr const char* kStutterEnabled = "stutter_on";
inline constexpr const char* kGateEnabled = "gate_on";
inline constexpr const char* kHighPassMidi = "hp_midi_on";
inline constexpr const char* kLowPassMidi = "lp_midi_on";
inline constexpr const char* kMidiVolume = "midi_volume";
inline constexpr const char* kMidiExpression = "midi_expression";
inline constexpr const char* kMidiBrightness = "midi_brightness";
inline constexpr const char* kSubOctave = "sub_octave";
inline constexpr const char* kClickPunch = "click_punch";
inline constexpr const char* kStereoWidth = "stereo_width";

inline constexpr const char* kClockSync = "clock_sync";
inline constexpr const char* kTempoBpm = "tempo_bpm";
inline constexpr const char* kTimeSignature = "time_sig";
inline constexpr const char* kAutoEnabled = "auto_on";
inline constexpr const char* kAutoBypass = "auto_bypass";
inline constexpr const char* kAutoSlot = "auto_slot";
inline constexpr const char* kAutoBars = "auto_bars";
inline constexpr const char* kAutoFillPreset = "auto_fill";
inline constexpr const char* kAutoGrid = "auto_grid";
inline constexpr const char* kAutoTailShorten = "auto_tail_shorten";
inline constexpr const char* kAutoCrushSweep = "auto_crush_sweep";

inline constexpr std::array<const char*, 5> kAutoSlotIds{
    kAutoBars, kAutoFillPreset, kAutoGrid, kAutoTailShorten, kAutoCrushSweep
};

inline constexpr const char* kReverseMode = "reverse_mode";
inline constexpr const char* kReverseMix = "reverse_mix";
inline constexpr const char* kDrive = "drive";
inline constexpr const char* kCrushRate = "crush_rate";
inline constexpr const char* kCrushBits = "crush_bits";
inline constexpr const char* kCrushMix = "crush_mix";
inline constexpr const char* kStutterRate = "stutter_rate";
inline constexpr const char* kStutterMix = "stutter_mix";
inline constexpr const char* kGateRate = "gate_rate";
inline constexpr const char* kGateMix = "gate_mix";
inline constexpr const char* kPumpRate = "pump_rate";
inline constexpr const char* kPumpMix = "pump_mix";
inline constexpr const char* kHighPass = "hp_cutoff";
inline constexpr const char* kLowPass = "lp_cutoff";
inline constexpr const char* kHighPassSyncEnabled = "hp_sync_on";
inline constexpr const char* kHighPassSyncRate = "hp_sync_rate";
inline constexpr const char* kLowPassSyncEnabled = "lp_sync_on";
inline constexpr const char* kLowPassSyncRate = "lp_sync_rate";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kLimiter = "limiter";
inline constexpr const char* kCeiling = "ceiling";
inline constexpr const char* kMasterBypass = "master_bypass";

inline constexpr std::array<std::pair<int, int>, 9> kTimeSignatures = { {
    { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
} };

[[nodiscard]] juce::String autoSlotId(int slot, const juce::String& baseId);
[[nodiscard]] juce::String layerId(int layer, const juce::String& baseId);
[[nodiscard]] double beatsPerBarForSignature(int index) noexcept;
[[nodiscard]] juce::String formatHz(float hz);
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedmkick::params
