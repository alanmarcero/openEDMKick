#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdint>
#include <utility>
#include <vector>

namespace openedmkick::presets {

struct FactoryPreset {
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
const std::vector<FactoryPreset>& fillBuildPresets();

void apply(
    const FactoryPreset& preset,
    juce::AudioProcessorValueTreeState& state,
    uint16_t sectionLockMask = 0);
void applyToCurrentAutoSlot(
    const FactoryPreset& preset,
    juce::AudioProcessorValueTreeState& state,
    uint16_t sectionLockMask = 0);

} // namespace openedmkick::presets
