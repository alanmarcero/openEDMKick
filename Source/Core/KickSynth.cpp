#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace openedmkick::core {
namespace {

constexpr float kMinHitMs = 15.0f;
constexpr float kMaxHitMs = 2000.0f;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

[[nodiscard]] float finiteClamp(float value, float fallback, float low, float high) noexcept {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}

[[nodiscard]] float nextNoise(dsp::Noise& noise) noexcept {
    return noise.next();
}

} // namespace

float noteFrequencyHz(int midiNote) noexcept {
    const int note = std::clamp(midiNote, 0, 127);
    return 440.0f * std::exp2((static_cast<float>(note) - 69.0f) / 12.0f);
}

std::string_view noteName(int midiNote) noexcept {
    constexpr std::array<std::string_view, 12> names{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int note = std::clamp(midiNote, 0, 127);
    return names[static_cast<size_t>(note % 12)];
}

int noteOctave(int midiNote) noexcept {
    return std::clamp(midiNote, 0, 127) / 12 - 1;
}

float shapedFall(float progress, float curve) noexcept {
    const float p = finiteClamp(progress, 1.0f, 0.0f, 1.0f);
    if (p <= 0.0f) return 1.0f;
    if (p >= 1.0f) return 0.0f;
    const float rate = std::exp2(2.0f + finiteClamp(curve, 0.0f, -1.0f, 1.0f) * 2.0f);
    const float end = std::exp(-rate);
    return std::clamp((std::exp(-rate * p) - end) / (1.0f - end), 0.0f, 1.0f);
}

float pitchOctavesAt(float timeMs, const KickSettings& settings) noexcept {
    const float time = std::max(0.0f, std::isfinite(timeMs) ? timeMs : 0.0f);
    const float sweepMs = finiteClamp(settings.sweepMs, 70.0f, 5.0f, 400.0f);
    const float snapMs = finiteClamp(settings.snapMs, 2.0f, 0.2f, 12.0f);
    const float sweep = finiteClamp(settings.sweepOctaves, 2.4f, 0.0f, 6.0f)
        * shapedFall(time / sweepMs, settings.sweepShape);
    const float snap = finiteClamp(settings.snapOctaves, 2.0f, 0.0f, 5.0f)
        * shapedFall(time / snapMs, 0.6f);
    return std::clamp(sweep + snap, 0.0f, 11.0f);
}

float amplitudeAt(float timeMs, const KickSettings& settings) noexcept {
    const float time = std::max(0.0f, std::isfinite(timeMs) ? timeMs : 0.0f);
    const float attack = finiteClamp(settings.attackMs, 0.15f, 0.05f, 10.0f);
    const float hold = finiteClamp(settings.holdMs, 45.0f, 0.0f, 400.0f);
    const float decay = finiteClamp(settings.decayMs, 300.0f, 1.0f, 1500.0f);
    if (time < attack) return time / attack;
    if (time < attack + hold) return 1.0f;
    return shapedFall((time - attack - hold) / decay, settings.decayShape);
}

float hitLengthMs(const KickSettings& settings) noexcept {
    const float attack = finiteClamp(settings.attackMs, 0.15f, 0.05f, 10.0f);
    const float hold = finiteClamp(settings.holdMs, 45.0f, 0.0f, 400.0f);
    const float decay = finiteClamp(settings.decayMs, 300.0f, 1.0f, 1500.0f);
    return std::clamp(attack + hold + decay, kMinHitMs, kMaxHitMs);
}

float saturate(float x, Saturation type) noexcept {
    const float input = std::clamp(std::isfinite(x) ? x : 0.0f, -32.0f, 32.0f);
    switch (type) {
        case Saturation::warm:
            return std::tanh(input * 1.2f) / std::tanh(1.2f);
        case Saturation::tube:
            return std::tanh(input * 2.0f) / std::tanh(2.0f);
        case Saturation::crunch:
            return std::clamp(input * 1.6f, -1.0f, 1.0f);
        case Saturation::fold:
            return 2.0f * std::asin(std::sin(input * 1.6f)) / std::numbers::pi_v<float>;
    }
    return input;
}

float driveGain(float drive, Saturation type) noexcept {
    const float amount = finiteClamp(drive, 0.55f, 0.0f, 1.0f);
    const float curveScale = type == Saturation::crunch ? 0.7f : (type == Saturation::fold ? 0.55f : 1.0f);
    return 1.0f + amount * 11.0f * curveScale;
}

void KickVoice::prepare(double hostRate) {
    hostRate_ = std::max(8000.0, std::isfinite(hostRate) ? hostRate : 48000.0);
    const int factor = dsp::oversamplingFactorFor(hostRate_);
    decimator_.prepare(factor);
    osRate_ = hostRate_ * factor;
    reverseBuffer_.reserve(static_cast<size_t>(std::ceil(hostRate_ * (kMaxHitMs / 1000.0))));
    reset();
}

void KickVoice::startState(const KickSettings& settings, int midiNote, float velocity,
                           const DrumLayerSettings& layer, float gainScale) noexcept {
    settings_ = settings;
    layer_ = layer;
    if (layer_.sound == DrumSoundType::kick) {
        const float layerDecay = finiteClamp(layer_.decayMs, 300.0f, 5.0f, 1500.0f);
        settings_.decayMs = finiteClamp(settings_.decayMs, 300.0f, 1.0f, 1500.0f) * (layerDecay / 300.0f);
        settings_.decayMs = finiteClamp(settings_.decayMs, 300.0f, 1.0f, 1500.0f);
    }
    active_ = true;
    reversing_ = false;
    reverseSamplesRemaining_ = 0;
    velocity_ = finiteClamp(velocity, 1.0f, 0.0f, 1.0f);
    const float layerGain = finiteClamp(layer_.gain, 1.0f, 0.0f, 1.0f);
    gain_ = std::pow(velocity_, 1.45f) * layerGain * finiteClamp(gainScale, 1.0f, 0.0f, 1.0f);
    const float transpose = finiteClamp(layer_.transposeSemitones, 0.0f, -24.0f, 24.0f);
    noteHz_ = noteFrequencyHz(midiNote) * std::exp2(transpose / 12.0f);
    if (layer_.sound == DrumSoundType::hat) noteHz_ = 6500.0f * std::exp2(transpose / 12.0f);
    noteHz_ = std::clamp(noteHz_, 1.0f, static_cast<float>(std::max(1000.0, osRate_ * 0.12)));
    phase_ = static_cast<double>(finiteClamp(settings_.startPhase, 0.5f, 0.0f, 1.0f) * 0.25f);
    subPhase_ = 0.0;
    clickPhase_ = 0.0;
    osIndex_ = 0;
    hostIndex_ = 0;
    float lengthMs = finiteClamp(layer_.decayMs, 180.0f, 5.0f, 1500.0f);
    if (layer_.sound == DrumSoundType::kick) lengthMs = hitLengthMs(settings_);
    hostLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::ceil(hostRate_ * lengthMs * 0.001f)));
    chokeRemaining_ = -1;
    chokeLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::ceil(hostRate_ * 0.003)));
    previousSample_ = 0.0f;
    decimator_.reset();
    knock_.reset();
    tone_.reset();
    lowCut_.reset();
    clickFilter_.reset();
    layerFilter_.reset();
    noise_.state = 0x9E3779B9u ^ static_cast<uint32_t>(std::clamp(midiNote, 0, 127) * 0x45D9F3Bu)
        ^ (static_cast<uint32_t>(layer_.sound) * 0x27D4EB2Du);
    const float safeKnockHz = finiteClamp(settings_.knockHz, 180.0f, 60.0f, 800.0f);
    const float safeKnockDb = finiteClamp(settings_.knockDb, 6.0f, 0.0f, 18.0f);
    const float safeTone = finiteClamp(settings_.toneHz, 9000.0f, 300.0f, 20000.0f);
    const float safeLowCut = finiteClamp(settings_.lowCutHz, 25.0f, 10.0f, 90.0f);
    float safeClickHz = finiteClamp(settings_.clickHz, 3500.0f, 500.0f, 14000.0f);
    if (layer_.sound == DrumSoundType::hat) safeClickHz = std::clamp(noteHz_, 2500.0f, 14000.0f);
    knock_.c = dsp::peakCoeffs(safeKnockHz, 1.05, safeKnockDb, osRate_);
    tone_.c = dsp::lowpassCoeffs(safeTone, 0.707, osRate_);
    lowCut_.c = dsp::highpassCoeffs(safeLowCut, 0.707, osRate_);
    clickFilter_.c = dsp::bandpassCoeffs(safeClickHz, 0.75, osRate_);
}

void KickVoice::start(const KickSettings& settings, int midiNote, float velocity,
                      const DrumLayerSettings& layer, float gainScale) noexcept {
    startState(settings, midiNote, velocity, layer, gainScale);
}

void KickVoice::startReverse(const KickSettings& settings, int midiNote, float velocity, int durationSamples,
                             const DrumLayerSettings& layer, float gainScale) noexcept {
    startState(settings, midiNote, velocity, layer, gainScale);
    reverseBuffer_.clear();
    reverseBuffer_.resize(static_cast<size_t>(hostLength_));
    const int sourceLength = static_cast<int>(reverseBuffer_.size());
    for (int i = 0; i < sourceLength; ++i) reverseBuffer_[static_cast<size_t>(i)] = renderHostSample();
    std::reverse(reverseBuffer_.begin(), reverseBuffer_.end());
    reversing_ = true;
    active_ = true;
    reverseSamplesRemaining_ = std::max(1, durationSamples);
    reverseReadPosition_ = 0.0;
    reverseReadStep_ = reverseSamplesRemaining_ > 1
        ? static_cast<double>(sourceLength - 1) / static_cast<double>(reverseSamplesRemaining_ - 1)
        : 0.0;
    hostIndex_ = 0;
}

void KickVoice::choke() noexcept {
    if (active_) chokeRemaining_ = chokeLength_;
}

void KickVoice::reset() noexcept {
    active_ = false;
    reversing_ = false;
    reverseSamplesRemaining_ = 0;
    reverseReadPosition_ = 0.0;
    reverseReadStep_ = 1.0;
    hostIndex_ = 0;
    osIndex_ = 0;
    chokeRemaining_ = -1;
    previousSample_ = 0.0f;
    reverseBuffer_.clear();
    decimator_.reset();
    knock_.reset();
    tone_.reset();
    lowCut_.reset();
    clickFilter_.reset();
    layerFilter_.reset();
}

float KickVoice::renderClick(float timeMs) noexcept {
    const float level = finiteClamp(settings_.clickLevel, 0.45f, 0.0f, 1.0f);
    if (level <= 0.0001f) return 0.0f;
    const float duration = finiteClamp(settings_.clickMs, 6.0f, 0.5f, 40.0f);
    if (timeMs >= duration) return 0.0f;
    const float hz = finiteClamp(settings_.clickHz, 3500.0f, 500.0f, 14000.0f);
    const float attack = std::min(1.0f, timeMs / 0.18f);
    const float envelope = std::exp(-timeMs * 5.0f / duration) * attack;
    const float noise = clickFilter_.process(nextNoise(noise_));
    float oscillatorHz = hz;
    if (settings_.clickType == ClickType::analog) {
        oscillatorHz *= 1.0f + 0.8f * shapedFall(timeMs / duration, 0.5f);
    }
    const float tone = std::sin(static_cast<float>(clickPhase_) * kTwoPi);
    clickPhase_ += static_cast<double>(oscillatorHz) / osRate_;
    clickPhase_ -= std::floor(clickPhase_);
    const float body = settings_.clickType == ClickType::noise ? 0.0f : tone;
    const float noiseMix = settings_.clickType == ClickType::beater ? 0.25f
        : (settings_.clickType == ClickType::analog ? 0.12f : 0.9f);
    return (body * (1.0f - noiseMix) + noise * noiseMix) * envelope * level * std::sqrt(velocity_);
}

float KickVoice::renderDrumLayer(float timeMs) noexcept {
    const float decayMs = finiteClamp(layer_.decayMs, 180.0f, 5.0f, 1500.0f);
    const float progress = std::clamp(timeMs / decayMs, 0.0f, 1.0f);
    const float decay = shapedFall(progress, 0.15f);
    const float bright = 0.45f + 0.55f * velocity_;
    const float noise = nextNoise(noise_);
    const float phase = static_cast<float>(phase_) * kTwoPi;

    switch (layer_.sound) {
        case DrumSoundType::snare: {
            const float noiseTail = std::exp(-timeMs * (3.8f + bright * 2.0f) / decayMs);
            const float body = std::sin(phase) * std::exp(-timeMs * 9.0f / decayMs);
            const float snareNoise = clickFilter_.process(noise);
            return decay * (body * 0.48f + snareNoise * noiseTail * (0.72f + bright * 0.2f));
        }
        case DrumSoundType::clap: {
            float burst = 0.0f;
            std::ranges::for_each(std::views::iota(0, 4), [&](int tap) {
                const float startMs = static_cast<float>(tap) * 8.0f;
                if (timeMs >= startMs) burst += std::exp(-(timeMs - startMs) * 1.25f);
            });
            const float clapNoise = clickFilter_.process(noise);
            const float diffuseTail = std::exp(-timeMs * 5.0f / decayMs);
            const float texture = std::min(1.0f, burst * 0.38f) * 0.78f + diffuseTail * 0.22f;
            return clapNoise * texture * decay * (0.8f + bright * 0.2f);
        }
        case DrumSoundType::hat: {
            const float metal = std::sin(static_cast<float>(phase_) * kTwoPi)
                + 0.72f * std::sin(static_cast<float>(phase_) * kTwoPi * 1.342f)
                + 0.51f * std::sin(static_cast<float>(phase_) * kTwoPi * 1.817f)
                + 0.38f * std::sin(static_cast<float>(phase_) * kTwoPi * 2.311f);
            const float hatNoise = clickFilter_.process(noise) * (0.85f + bright * 0.3f);
            return (hatNoise * 0.72f + metal * 0.16f) * decay;
        }
        case DrumSoundType::kick:
            break;
    }
    return 0.0f;
}

float KickVoice::renderOversampled() noexcept {
    const float timeMs = static_cast<float>(1000.0 * static_cast<double>(osIndex_) / osRate_);
    float sample = 0.0f;
    if (layer_.sound == DrumSoundType::kick) {
        const float envelope = amplitudeAt(timeMs, settings_);
        const float semitoneRatio = std::exp2(pitchOctavesAt(timeMs, settings_));
        const float frequency = std::min(noteHz_ * semitoneRatio, static_cast<float>(osRate_ * 0.45));
        const float bodyPhase = static_cast<float>(phase_) * kTwoPi;
        const float tone = finiteClamp(settings_.bodyTone, 0.65f, 0.0f, 1.0f);
        const float fundamental = std::sin(bodyPhase);
        const float harmonic2 = std::sin(bodyPhase * 2.0f);
        const float harmonic3 = std::sin(bodyPhase * 3.0f);
        const float harmonic4 = std::sin(bodyPhase * 4.0f);
        float body = fundamental * (1.0f - tone * 0.24f)
            + tone * (0.40f * harmonic2 + 0.22f * harmonic3 + 0.12f * harmonic4);
        const float knockGain = dsp::dbToGain(finiteClamp(settings_.knockDb, 6.0f, 0.0f, 18.0f));
        body += knock_.process(body) * (knockGain - 1.0f) * 0.28f;

        const float subLevel = finiteClamp(settings_.subLevel, 0.35f, 0.0f, 1.0f);
        const float sub = std::sin(static_cast<float>(subPhase_) * kTwoPi) * subLevel;
        const float drive = finiteClamp(settings_.drive, 0.55f, 0.0f, 1.0f);
        const float grit = finiteClamp(settings_.grit, 0.35f, 0.0f, 1.0f);
        const float attackMs = finiteClamp(settings_.attackMs, 0.15f, 0.05f, 10.0f);
        const float holdMs = finiteClamp(settings_.holdMs, 45.0f, 0.0f, 400.0f);
        const float decayMs = finiteClamp(settings_.decayMs, 300.0f, 1.0f, 1500.0f);
        const float decayProgress = std::clamp((timeMs - attackMs - holdMs) / decayMs, 0.0f, 1.0f);
        const float velocityDrive = 0.55f + 0.45f * velocity_;
        const float currentDrive = drive * velocityDrive * (1.0f - (1.0f - grit) * decayProgress);
        const float driveAmount = driveGain(currentDrive, settings_.saturation);
        const float shaped = saturate(body * driveAmount, settings_.saturation) / std::sqrt(driveAmount);
        const float mixed = shaped * (1.0f - subLevel * 0.45f) + sub * 0.45f;
        const float click = renderClick(timeMs);
        sample = tone_.process(mixed + click);
        sample = lowCut_.process(sample);
        const float clip = finiteClamp(settings_.clip, 0.45f, 0.0f, 1.0f);
        sample += clip * (dsp::softClip(sample * 1.35f) - sample);
        sample *= envelope;
        phase_ += static_cast<double>(frequency) / osRate_;
    }
    if (layer_.sound != DrumSoundType::kick) {
        sample = renderDrumLayer(timeMs);
        sample = tone_.process(sample);
        sample = lowCut_.process(sample);
        phase_ += static_cast<double>(noteHz_) / osRate_;
    }
    sample *= gain_;

    phase_ -= std::floor(phase_);
    subPhase_ += static_cast<double>(noteHz_) / osRate_;
    subPhase_ -= std::floor(subPhase_);
    ++osIndex_;
    return std::isfinite(sample) ? sample : 0.0f;
}

float KickVoice::renderHostSample() noexcept {
    if (!active_ || hostIndex_ >= hostLength_) {
        active_ = false;
        return 0.0f;
    }
    std::array<float, dsp::Decimator::kMaxFactor> samples{};
    for (int i = 0; i < decimator_.factor(); ++i) samples[static_cast<size_t>(i)] = renderOversampled();
    float sample = decimator_.process(samples);
    sample = layerFilter_.process(sample, layer_.highPassHz, layer_.lowPassHz, hostRate_);
    if (chokeRemaining_ >= 0) {
        sample *= static_cast<float>(chokeRemaining_) / static_cast<float>(chokeLength_);
        if (--chokeRemaining_ <= 0) active_ = false;
    }
    previousSample_ = std::isfinite(sample) ? sample : 0.0f;
    ++hostIndex_;
    if (hostIndex_ >= hostLength_) active_ = false;
    return previousSample_;
}

float KickVoice::nextSample() noexcept {
    if (!active_) return 0.0f;
    if (!reversing_) return renderHostSample();
    if (reverseSamplesRemaining_ <= 0 || reverseBuffer_.empty()) {
        active_ = false;
        reversing_ = false;
        return 0.0f;
    }
    const size_t last = reverseBuffer_.size() - 1u;
    const size_t index = std::min(last, static_cast<size_t>(reverseReadPosition_));
    const size_t next = std::min(last, index + 1u);
    const float fraction = static_cast<float>(reverseReadPosition_ - static_cast<double>(index));
    float sample = reverseBuffer_[index] + fraction * (reverseBuffer_[next] - reverseBuffer_[index]);
    if (chokeRemaining_ >= 0) {
        sample *= static_cast<float>(chokeRemaining_) / static_cast<float>(chokeLength_);
        if (--chokeRemaining_ <= 0) active_ = false;
    }
    reverseReadPosition_ += reverseReadStep_;
    --reverseSamplesRemaining_;
    if (reverseSamplesRemaining_ <= 0) {
        active_ = false;
        reversing_ = false;
    }
    previousSample_ = std::isfinite(sample) ? sample : 0.0f;
    return previousSample_;
}

std::vector<float> renderKick(const KickSettings& settings, double sampleRate, int midiNote) {
    KickVoice voice;
    voice.prepare(sampleRate);
    voice.start(settings, midiNote, 1.0f);
    std::vector<float> samples;
    samples.reserve(static_cast<size_t>(std::ceil(sampleRate * hitLengthMs(settings) * 0.001)));
    while (voice.isActive()) samples.push_back(voice.nextSample());
    return samples;
}

std::vector<float> renderLayeredKick(const KickSettings& settings,
                                    const std::array<DrumLayerSettings, kNumSoundLayers>& layers,
                                    double sampleRate, int midiNote) {
    std::array<KickVoice, kNumSoundLayers> voices;
    float lengthMs = hitLengthMs(settings);
    std::ranges::for_each(std::views::iota(size_t{ 0 }, voices.size()), [&](size_t index) {
        voices[index].prepare(sampleRate);
        const auto& layer = layers[index];
        if (!layer.enabled || layer.gain <= 0.0001f) return;
        voices[index].start(settings, midiNote, 1.0f, layer);
        float layerLength = finiteClamp(layer.decayMs, 180.0f, 5.0f, 1500.0f);
        if (layer.sound == DrumSoundType::kick)
            layerLength = hitLengthMs(settings) * finiteClamp(layer.decayMs, 300.0f, 5.0f, 1500.0f) / 300.0f;
        lengthMs = std::max(lengthMs, std::min(2000.0f, layerLength));
    });

    std::vector<float> samples;
    samples.reserve(static_cast<size_t>(std::ceil(sampleRate * lengthMs * 0.001f)));
    while (std::ranges::any_of(voices, [](const KickVoice& voice) { return voice.isActive(); })) {
        float sample = 0.0f;
        std::ranges::for_each(voices, [&](KickVoice& voice) { sample += voice.nextSample(); });
        samples.push_back(std::isfinite(sample) ? sample : 0.0f);
    }
    return samples;
}

} // namespace openedmkick::core
