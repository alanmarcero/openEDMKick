#pragma once

#include "AutoSequencer.h"
#include <array>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>

namespace openedmkick::params {

// Kick drum synthesis & 12-note tuning parameters
inline constexpr const char* kNote = "note";
inline constexpr const char* kOctave = "octave";
inline constexpr const char* kFineTune = "fine_tune";
inline constexpr const char* kStartPitch = "start_pitch";
inline constexpr const char* kPitchDecay = "pitch_decay";
inline constexpr const char* kPitchCurve = "pitch_curve";
inline constexpr const char* kTailLength = "tail_length";
inline constexpr const char* kTailCurve = "tail_curve";
inline constexpr const char* kSubDrive = "sub_drive";
inline constexpr const char* kClickLevel = "click_level";
inline constexpr const char* kClickFreq = "click_freq";
inline constexpr const char* kPunch = "punch";
inline constexpr const char* kSnareLevel = "snare_level";
inline constexpr const char* kSnareTone = "snare_tone";
inline constexpr const char* kSnareDecay = "snare_decay";
inline constexpr const char* kClapLevel = "clap_level";
inline constexpr const char* kClapSpread = "clap_spread";
inline constexpr const char* kClapTone = "clap_tone";
inline constexpr const char* kHatLevel = "hat_level";
inline constexpr const char* kHatTone = "hat_tone";
inline constexpr const char* kHatDecay = "hat_decay";
inline constexpr const char* kLockIn = "lock_in";

// Clock sync & transport parameters
inline constexpr const char* kClockSync = "clock_sync";
inline constexpr const char* kTempoBpm = "tempo_bpm";
inline constexpr const char* kTimeSignature = "time_sig";

// A/B/C/D Auto Mode & 1/64 Piano Roll parameters
inline constexpr const char* kAutoEnabled = "auto_on";
inline constexpr const char* kAutoBypass = "auto_bypass";
inline constexpr const char* kAutoSlot = "auto_slot";
inline constexpr const char* kAutoBars = "auto_bars";
inline constexpr const char* kAutoFillPreset = "auto_fill";
inline constexpr const char* kAutoGrid = "auto_grid";
inline constexpr const char* kAutoTailShorten = "auto_tail_shorten";
inline constexpr const char* kAutoPitchRise = "auto_pitch_rise";
inline constexpr const char* kAutoCrushSweep = "auto_crush_sweep";

inline constexpr std::array<const char*, 6> kAutoSlotIds{
    kAutoBars, kAutoFillPreset, kAutoGrid, kAutoTailShorten, kAutoPitchRise, kAutoCrushSweep
};

// Host-synced Effects & Output parameters
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
inline constexpr const char* kOutput = "output";
inline constexpr const char* kLimiter = "limiter";
inline constexpr const char* kCeiling = "ceiling";
inline constexpr const char* kMasterBypass = "master_bypass";

inline constexpr std::array<std::pair<int, int>, 9> kTimeSignatures = { {
    { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
} };

[[nodiscard]] juce::String autoSlotId(int slot, const juce::String& baseId);
[[nodiscard]] double beatsPerBarForSignature(int index) noexcept;
[[nodiscard]] juce::String formatHz(float hz);
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedmkick::params
