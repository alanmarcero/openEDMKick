#include "WavFile.h"

#include <algorithm>
#include <cmath>

namespace openedmkick::suite {
namespace {

void write16(juce::OutputStream& out, uint16_t value) {
    out.writeByte(static_cast<char>(value & 0xff));
    out.writeByte(static_cast<char>((value >> 8) & 0xff));
}

void write32(juce::OutputStream& out, uint32_t value) {
    out.writeByte(static_cast<char>(value & 0xff));
    out.writeByte(static_cast<char>((value >> 8) & 0xff));
    out.writeByte(static_cast<char>((value >> 16) & 0xff));
    out.writeByte(static_cast<char>((value >> 24) & 0xff));
}

} // namespace

bool writeStereoWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate, int bits) {
    if (buffer.getNumSamples() <= 0 || sampleRate < 1000.0) return false;
    const int depth = bits >= 24 ? 24 : 16;
    const int channels = 2;
    const int bytesPerSample = depth / 8;
    const auto dataBytes = static_cast<uint32_t>(buffer.getNumSamples() * channels * bytesPerSample);
    file.getParentDirectory().createDirectory();
    auto stream = file.createOutputStream();
    if (stream == nullptr) return false;
    stream->setPosition(0);
    stream->truncate();
    stream->write("RIFF", 4);
    write32(*stream, 36u + dataBytes);
    stream->write("WAVE", 4);
    stream->write("fmt ", 4);
    write32(*stream, 16);
    write16(*stream, 1);
    write16(*stream, static_cast<uint16_t>(channels));
    const auto rate = static_cast<uint32_t>(std::lround(sampleRate));
    write32(*stream, rate);
    write32(*stream, rate * static_cast<uint32_t>(channels * bytesPerSample));
    write16(*stream, static_cast<uint16_t>(channels * bytesPerSample));
    write16(*stream, static_cast<uint16_t>(depth));
    stream->write("data", 4);
    write32(*stream, dataBytes);
    const int scale = depth == 16 ? 32767 : 8388607;
    const int minimum = depth == 16 ? -32768 : -8388608;
    for (int index = 0; index < buffer.getNumSamples(); ++index) {
        for (int channel = 0; channel < channels; ++channel) {
            const int source = std::min(channel, buffer.getNumChannels() - 1);
            const float sample = source >= 0 ? buffer.getSample(source, index) : 0.0f;
            int value = static_cast<int>(std::lround(std::clamp(sample, -1.0f, 1.0f) * static_cast<float>(scale)));
            value = std::clamp(value, minimum, scale);
            stream->writeByte(static_cast<char>(value & 0xff));
            stream->writeByte(static_cast<char>((value >> 8) & 0xff));
            if (depth == 24) stream->writeByte(static_cast<char>((value >> 16) & 0xff));
        }
    }
    stream->flush();
    return stream->getStatus().wasOk();
}

} // namespace openedmkick::suite
