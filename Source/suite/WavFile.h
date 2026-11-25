#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace suite {

// Writes a stereo PCM WAV. bits is 16 or 24.
bool writeStereoWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate, int bits);

} // namespace suite
