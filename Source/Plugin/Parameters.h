#pragma once

#include "../suite/EqNotes.h"
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
inline constexpr const char* kKnockHz = "knock_hz";
inline constexpr const char* kKnockDb = "knock_db";
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

inline constexpr const char* kAmpEnabled = "amp_enabled";
inline constexpr const char* kAmpAttack = "amp_attack_ms";
inline constexpr const char* kAmpDecay = "amp_decay_ms";
inline constexpr const char* kAmpSustain = "amp_sustain";
inline constexpr const char* kAmpRelease = "amp_release_ms";
inline constexpr const char* kReverseEnabled = "reverse_on";
inline constexpr const char* kDistType = "dist_type";
inline constexpr const char* kDistDrive = "dist_drive";
inline constexpr const char* kDistTone = "dist_tone";
inline constexpr const char* kDistMix = "dist_mix";
inline constexpr const char* kCompThresh = "comp_thresh";
inline constexpr const char* kCompRatio = "comp_ratio";
inline constexpr const char* kCompAttack = "comp_attack";
inline constexpr const char* kCompRelease = "comp_release";
inline constexpr const char* kCompMakeup = "comp_makeup";
inline constexpr const char* kCompMix = "comp_mix";
inline constexpr const char* kReverbSize = "reverb_size";
inline constexpr const char* kReverbDecay = "reverb_decay";
inline constexpr const char* kReverbDamp = "reverb_damp";
inline constexpr const char* kReverbGate = "reverb_gate";
inline constexpr const char* kReverbMix = "reverb_mix";
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

inline constexpr const char* kReverseMode = "reverse_mode";
inline constexpr const char* kReverseMix = "reverse_mix";
inline constexpr const char* kHighPass = "hp_cutoff";
inline constexpr const char* kLowPass = "lp_cutoff";
inline constexpr const char* kHighPassSyncEnabled = "hp_sync_on";
inline constexpr const char* kHighPassSyncRate = "hp_sync_rate";
inline constexpr const char* kLowPassSyncEnabled = "lp_sync_on";
inline constexpr const char* kLowPassSyncRate = "lp_sync_rate";
inline constexpr const char* kOutput = "output_gain";
inline constexpr const char* kLimiterEnabled = "limiter_enabled";
inline constexpr const char* kKeyTracking = "key_tracking";
inline constexpr const char* kCeiling = "limiter_ceiling";
inline constexpr const char* kEqLowFreq = "eq_low_freq";
inline constexpr const char* kEqLowDb = "eq_low_db";
inline constexpr const char* kEqMidFreq = "eq_mid_freq";
inline constexpr const char* kEqMidDb = "eq_mid_db";
inline constexpr const char* kEqHighFreq = "eq_high_freq";
inline constexpr const char* kEqHighDb = "eq_high_db";

inline constexpr std::array<std::pair<int, int>, 9> kTimeSignatures = { {
    { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
} };

inline constexpr int kEqLowMinMidi = suite::eq::kLowMinMidi;
inline constexpr int kEqLowMaxMidi = suite::eq::kLowMaxMidi;
inline constexpr int kEqMidMinMidi = suite::eq::kMidMinMidi;
inline constexpr int kEqMidMaxMidi = suite::eq::kMidMaxMidi;
inline constexpr int kEqHighMinMidi = suite::eq::kHighMinMidi;
inline constexpr int kEqHighMaxMidi = suite::eq::kHighMaxMidi;

[[nodiscard]] std::pair<int, int> eqBandMidiLimits(const juce::String& freqParamId) noexcept;
[[nodiscard]] double beatsPerBarForSignature(int index) noexcept;
[[nodiscard]] juce::String formatHz(float hz);
[[nodiscard]] float eqNoteToHz(int midiNote) noexcept;
[[nodiscard]] int eqHzToNearestMidi(float hz, int minMidi, int maxMidi) noexcept;
[[nodiscard]] juce::String formatEqNote(float hz, int minMidi, int maxMidi);
[[nodiscard]] float parseEqNoteOrHz(const juce::String& text, int minMidi, int maxMidi);
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedmkick::params
