#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>

namespace openedmkick::presets {

// Clap, hat, and kick banks are 56 programs. The snare bank is 84.
inline constexpr int kFactoryProgramCount = 56;

inline constexpr int kCatalogVersion = 7;

struct FactoryPreset {
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
const std::vector<FactoryPreset>& fillBuildPresets();
void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state);
void applyToCurrentAutoSlot(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state);

} // namespace openedmkick::presets
