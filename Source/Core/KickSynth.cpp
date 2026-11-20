#include "KickSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>
#include <ranges>

namespace openedmkick::core {
namespace {

constexpr float kMinHitMs = 15.0f;
constexpr float kMaxHitMs = 2000.0f;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

[[nodiscard]] float finiteClamp(float value, float fallback, float low, float high) noexcept {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
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
    if (progress >= 1.0f) return 0.0f;
    if (progress <= 0.0f) return 1.0f;
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
    if (time >= sweepMs && time >= snapMs) return 0.0f;
    const float sweep = finiteClamp(settings.sweepOctaves, 2.4f, 0.0f, 6.0f)
        * shapedFall(time / sweepMs, settings.sweepShape);
    const float snap = finiteClamp(settings.snapOctaves, 2.0f, 0.0f, 5.0f)
        * shapedFall(time / snapMs, 0.6f);
    return std::clamp(sweep + snap, 0.0f, 11.0f);
}

double integratePitchCycles(double startMs, double endMs, float noteHz, double maxHz, const KickSettings& settings) noexcept {
    const double t0 = std::max(0.0, std::isfinite(startMs) ? startMs : 0.0);
    const double t1 = std::max(t0, std::isfinite(endMs) ? endMs : t0);
    const double totalSeconds = (t1 - t0) * 0.001;
    if (totalSeconds <= 0.0) return 0.0;
    const double safeBaseHz = std::clamp(static_cast<double>(noteHz), 1.0, maxHz);
    const double sweepEndMs = finiteClamp(settings.sweepMs, 70.0f, 5.0f, 400.0f);
    const double snapEndMs = finiteClamp(settings.snapMs, 2.0f, 0.2f, 12.0f);
    if (t0 >= std::max(sweepEndMs, snapEndMs)) return safeBaseHz * totalSeconds;

    const int substeps = std::clamp(static_cast<int>(std::ceil((t1 - t0) / 0.02)), 2, 256);
    const double stepMs = (t1 - t0) / static_cast<double>(substeps);
    const double stepSeconds = stepMs * 0.001;
    const auto freqAt = [&](double ms) noexcept {
        const double oct = static_cast<double>(pitchOctavesAt(static_cast<float>(ms), settings));
        return std::min(maxHz, safeBaseHz * std::exp2(oct));
    };
    double fPrev = freqAt(t0);
    double cycles = 0.0;
    int step = 0;
    while (step < substeps) {
        ++step;
        const double fNext = freqAt(t0 + stepMs * static_cast<double>(step));
        const double diff = fNext - fPrev;
        double meanHz = 0.5 * (fPrev + fNext);
        if (std::abs(diff) > 1.0e-6 * fPrev) meanHz = diff / std::log(fNext / fPrev);
        cycles += meanHz * stepSeconds;
        fPrev = fNext;
    }
    return cycles;
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

void KickVoice::prepare(double hostRate, bool allowReverse) {
    hostRate_ = std::max(8000.0, std::isfinite(hostRate) ? hostRate : 48000.0);
    const int factor = dsp::oversamplingFactorFor(hostRate_);
    decimator_.prepare(factor);
    osRate_ = hostRate_ * factor;
    const size_t cacheRate = static_cast<size_t>(std::ceil(std::min(hostRate_, 48000.0) * (kMaxHitMs * 0.001))) + 8u;
    reverseCache_.assign(allowReverse ? cacheRate : 0u, 0.0f);
    reset();
}

void KickVoice::startState(const KickSettings& settings, int midiNote, float velocity,
                           const DrumLayerSettings& layer, float gainScale) noexcept {
    const float carried = active_ ? previousSample_ : 0.0f;
    const bool resume = active_;
    settings_ = settings;
    const float transpose = finiteClamp(layer.transposeSemitones, 0, -24, 24);
    const float pitchRatio = noteFrequencyHz(midiNote) / noteFrequencyHz(kDefaultMidiNote) * std::exp2(transpose / 12.0f);
    const auto track = [&](float hz) { return std::clamp(hz * pitchRatio, 5.0f, static_cast<float>(hostRate_ * 0.45)); };
    layer_ = layer;
    if (layer_.highPassHz > ToneFilter::kHighPassBypassHz + 0.5f) layer_.highPassHz = track(layer_.highPassHz);
    if (layer_.lowPassHz < ToneFilter::kLowPassBypassHz - 1) layer_.lowPassHz = track(layer_.lowPassHz);
    const float toneScale = finiteClamp(layer.toneScale, 1, 0.1f, 2);
    settings_.toneHz *= toneScale;
    settings_.clickHz *= toneScale;
    const float sweepScale = finiteClamp(layer.sweepScale, 1, 0, 2);
    settings_.sweepOctaves *= sweepScale;
    settings_.snapOctaves *= sweepScale;
    if (layer_.sound == DrumSoundType::kick) {
        const float layerDecay = finiteClamp(layer_.decayMs, 300.0f, 5.0f, 1500.0f);
        settings_.decayMs = finiteClamp(settings_.decayMs, 300.0f, 1.0f, 1500.0f) * (layerDecay / 300.0f);
        settings_.decayMs = finiteClamp(settings_.decayMs, 300.0f, 1.0f, 1500.0f);
    }
    active_ = true;
    reversing_ = false;
    reverseTotalSamples_ = 0;
    reverseSamplesRemaining_ = 0;
    reverseCacheLength_ = 0;
    const float hitVelocity = finiteClamp(velocity, 1.0f, 0.0f, 1.0f);
    velocity_ = 1.0f - finiteClamp(layer.velocityTimbre, 1, 0, 1) * (1.0f - hitVelocity);
    const float layerGain = finiteClamp(layer_.gain, 1.0f, 0.0f, 1.0f);
    gain_ = std::pow(hitVelocity, finiteClamp(layer.velocityCurve, 1.45f, 0.1f, 4)) * layerGain * finiteClamp(gainScale, 1.0f, 0.0f, 1.0f);
    noteHz_ = noteFrequencyHz(midiNote) * std::exp2(transpose / 12.0f);
    if (layer_.sound == DrumSoundType::snare) noteHz_ = 195.0f * std::exp2(transpose / 12.0f);
    if (layer_.sound == DrumSoundType::hat) noteHz_ = 6500.0f * pitchRatio;
    noteHz_ = std::clamp(noteHz_, 1.0f, static_cast<float>(std::max(1000.0, osRate_ * 0.12)));
    phase_ = static_cast<double>(finiteClamp(settings_.startPhase, 0.5f, 0.0f, 1.0f) * 0.25f);
    subPhase_ = phase_;
    subOctavePhase_ = phase_ * 0.5;
    clickModalPhases_.fill(0.0);
    modalPhases_.fill(0.0);
    snareComb_.fill(0.0f);
    snareCombIndex_ = 0;
    snareCombLp_ = 0.0f;
    osIndex_ = 0;
    ampIndex_ = 0;
    releaseStart_ = -1;
    releaseLength_ = static_cast<int64_t>(std::ceil(finiteClamp(settings_.ampReleaseMs, 300, 0, 20000) * osRate_ * 0.001));
    hostIndex_ = 0;
    float lengthMs = finiteClamp(layer_.decayMs, 180.0f, 5.0f, 1500.0f);
    if (layer_.sound == DrumSoundType::kick) lengthMs = hitLengthMs(settings_);
    hostLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::ceil(hostRate_ * lengthMs * 0.001f)));
    if (settings_.ampEnabled) hostLength_ = std::numeric_limits<int64_t>::max();
    chokeRemaining_ = -1;
    chokeLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::ceil(hostRate_ * 0.003)));
    declickFrom_ = 0.0f;
    declickLeft_ = 0;
    if (resume && std::abs(carried) > 1.0e-5f) {
        declickFrom_ = carried;
        declickLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::lround(hostRate_ * 0.0024)));
        declickLeft_ = declickLength_;
    }
    previousSample_ = 0.0f;
    decimator_.reset();
    knock_.reset();
    tone_.reset();
    lowCut_.reset();
    clickFilter_.reset();
    saturator_.reset();
    layerFilter_.reset();
    const uint32_t seedValue = 0x9E3779B9u ^ static_cast<uint32_t>(std::clamp(midiNote, 0, 127) * 0x45D9F3Bu)
        ^ (static_cast<uint32_t>(layer_.sound) * 0x27D4EB2Du);
    noise_.seed(seedValue);
    knockHz_ = track(finiteClamp(settings_.knockHz, 180.0f, 60.0f, 800.0f));
    knockDb_ = finiteClamp(settings_.knockDb, 6.0f, 0.0f, 18.0f);
    const float safeTone = track(finiteClamp(settings_.toneHz, 9000.0f, 300.0f, 20000.0f));
    const float safeLowCut = track(finiteClamp(settings_.lowCutHz, 25.0f, 10.0f, 90.0f));
    clickFrequencyHz_ = track(finiteClamp(settings_.clickHz, 3500.0f, 500.0f, 14000.0f));
    float safeClickHz = clickFrequencyHz_;
    if (layer_.sound == DrumSoundType::snare) safeClickHz = std::clamp(2600.0f * toneScale * std::exp2(transpose / 12.0f), 200.0f, static_cast<float>(hostRate_ * 0.45));
    if (layer_.sound == DrumSoundType::clap) safeClickHz = std::clamp(1650.0f * toneScale * std::exp2(transpose / 12.0f), 200.0f, static_cast<float>(hostRate_ * 0.45));
    if (layer_.sound == DrumSoundType::hat) safeClickHz = std::clamp(noteHz_ * toneScale, 5.0f, static_cast<float>(hostRate_ * 0.45));
    knock_.c = dsp::peakCoeffs(knockHz_, 1.05, knockDb_, osRate_);
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
    const float carried = active_ ? previousSample_ : 0.0f;
    const bool resume = active_;
    if (reverseCache_.empty()) {
        active_ = false;
        return;
    }

    const double savedHostRate = hostRate_;
    const int savedFactor = decimator_.factor();
    if (savedHostRate > 48000.0) {
        const double fastCacheRate = savedHostRate >= 96000.0 ? 16000.0 : 32000.0;
        hostRate_ = fastCacheRate;
        decimator_.prepare(1);
        osRate_ = fastCacheRate;
    }

    KickSettings forwardSettings = settings;
    forwardSettings.ampEnabled = false;
    startState(forwardSettings, midiNote, velocity, layer, gainScale);
    declickLeft_ = 0;
    declickFrom_ = 0.0f;
    const int maxCache = static_cast<int>(reverseCache_.size());
    const int targetSamples = std::clamp(static_cast<int>(hostLength_), 1, maxCache);
    hostLength_ = targetSamples;
    int written = 0;
    while (written < targetSamples && active_) {
        reverseCache_[static_cast<size_t>(written)] = renderHostSample();
        ++written;
    }
    reverseCacheLength_ = std::max(1, written);

    if (savedHostRate > 48000.0) {
        hostRate_ = savedHostRate;
        decimator_.prepare(savedFactor);
        osRate_ = savedHostRate * static_cast<double>(decimator_.factor());
    }

    settings_.ampEnabled = settings.ampEnabled;
    settings_.ampAttackMs = settings.ampAttackMs;
    settings_.ampDecayMs = settings.ampDecayMs;
    settings_.ampSustain = settings.ampSustain;
    settings_.ampReleaseMs = settings.ampReleaseMs;
    ampIndex_ = 0;
    releaseStart_ = -1;
    releaseLength_ = static_cast<int64_t>(std::ceil(finiteClamp(settings_.ampReleaseMs, 300, 0, 20000) * osRate_ * 0.001));
    chokeRemaining_ = -1;
    previousSample_ = 0.0f;
    declickFrom_ = 0.0f;
    declickLeft_ = 0;
    if (resume && std::abs(carried) > 1.0e-5f) {
        declickFrom_ = carried;
        declickLength_ = std::max<int64_t>(1, static_cast<int64_t>(std::lround(hostRate_ * 0.0024)));
        declickLeft_ = declickLength_;
    }
    reversing_ = true;
    active_ = true;
    reverseSourcePosition_ = static_cast<double>(reverseCacheLength_ - 1);
    reverseTotalSamples_ = std::max(1, durationSamples);
    retimeReverse(durationSamples);
}

void KickVoice::retimeReverse(int remainingSamples) noexcept {
    if (!reversing_ || !active_) return;
    reverseSamplesRemaining_ = std::max(1, remainingSamples);
    reverseTotalSamples_ = std::max(reverseTotalSamples_, reverseSamplesRemaining_);
    reverseSourceStep_ = 0.0;
    if (reverseSamplesRemaining_ > 1) {
        reverseSourceStep_ = reverseSourcePosition_ / static_cast<double>(reverseSamplesRemaining_ - 1);
    }
}

float KickVoice::ampLevel() const noexcept {
    if (releaseStart_ >= 0) {
        if (releaseLength_ <= 0) return 0;
        return releaseLevel_ * std::clamp(1.0f - static_cast<float>(ampIndex_ - releaseStart_) / static_cast<float>(releaseLength_), 0.0f, 1.0f);
    }
    const double attack = finiteClamp(settings_.ampAttackMs, 0, 0, 10000) * osRate_ * 0.001;
    const double decay = finiteClamp(settings_.ampDecayMs, 100, 0, 10000) * osRate_ * 0.001;
    const float sustain = finiteClamp(settings_.ampSustain, 0.8f, 0, 1);
    if (static_cast<double>(ampIndex_) < attack) return static_cast<float>(ampIndex_ / attack);
    if (static_cast<double>(ampIndex_) < attack + decay)
        return 1.0f + (sustain - 1.0f) * static_cast<float>((ampIndex_ - attack) / decay);
    return sustain;
}

void KickVoice::release() noexcept {
    if (!settings_.ampEnabled || !active_ || releaseStart_ >= 0) return;
    releaseLevel_ = ampLevel();
    releaseStart_ = ampIndex_;
    if (releaseLength_ == 0) active_ = false;
}

double KickVoice::remainingSeconds() const noexcept {
    if (!active_) return 0;
    if (reversing_) return reverseSamplesRemaining_ / hostRate_;
    if (!settings_.ampEnabled) return static_cast<double>(hostLength_ - hostIndex_) / hostRate_;
    if (releaseStart_ < 0) return static_cast<double>(releaseLength_) / osRate_;
    return std::max(0.0, static_cast<double>(releaseLength_ - (ampIndex_ - releaseStart_)) / osRate_);
}

void KickVoice::choke() noexcept {
    if (active_) chokeRemaining_ = chokeLength_;
}

void KickVoice::reset() noexcept {
    active_ = false;
    reversing_ = false;
    reverseTotalSamples_ = 0;
    reverseSamplesRemaining_ = 0;
    reverseCacheLength_ = 0;
    reverseSourcePosition_ = 0;
    reverseSourceStep_ = 1;
    hostIndex_ = 0;
    osIndex_ = 0;
    chokeRemaining_ = -1;
    declickLeft_ = 0;
    declickFrom_ = 0.0f;
    previousSample_ = 0.0f;
    clickModalPhases_.fill(0.0);
    modalPhases_.fill(0.0);
    snareComb_.fill(0.0f);
    snareCombIndex_ = 0;
    snareCombLp_ = 0.0f;
    decimator_.reset();
    knock_.reset();
    tone_.reset();
    lowCut_.reset();
    clickFilter_.reset();
    saturator_.reset();
    layerFilter_.reset();
}

float KickVoice::renderClick(float timeMs) noexcept {
    const float level = finiteClamp(settings_.clickLevel, 0.45f, 0.0f, 1.0f);
    if (level <= 0.0001f) return 0.0f;
    const float duration = finiteClamp(settings_.clickMs, 6.0f, 0.5f, 40.0f);
    if (timeMs >= duration) return 0.0f;
    const float attack = std::min(1.0f, timeMs / 0.14f);
    const float nyquist = static_cast<float>(osRate_ * 0.45);
    const float baseHz = std::clamp(clickFrequencyHz_, 500.0f, 8000.0f);
    float pitchDrop = 1.0f;
    if (settings_.clickType == ClickType::analog) pitchDrop += 0.8f * shapedFall(timeMs / duration, 0.5f);

    constexpr std::array<float, 4> modalRatios{ 1.0f, 1.593f, 2.136f, 2.917f };
    constexpr std::array<float, 4> modalGains{ 0.52f, 0.34f, 0.24f, 0.16f };
    constexpr std::array<float, 4> modalDecays{ 4.6f, 6.2f, 8.0f, 10.5f };
    float modalSum = 0.0f;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, clickModalPhases_.size()), [&](size_t mode) {
        const float modeHz = baseHz * modalRatios[mode] * pitchDrop;
        if (modeHz >= nyquist) return;
        const float margin = std::clamp((nyquist - modeHz) / (nyquist * 0.15f), 0.0f, 1.0f);
        const float bandFade = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * margin);
        const float modeEnv = std::exp(-timeMs * modalDecays[mode] / duration) * attack;
        modalSum += modalGains[mode] * modeEnv * bandFade * std::sin(static_cast<float>(clickModalPhases_[mode]) * kTwoPi);
        clickModalPhases_[mode] += static_cast<double>(modeHz) / osRate_;
        clickModalPhases_[mode] -= std::floor(clickModalPhases_[mode]);
    });

    const float osScale = std::sqrt(static_cast<float>(std::max(1, decimator_.factor())));
    const float pink = noise_.nextPink();
    const float velvetEnv = std::exp(-timeMs * 9.0f / duration);
    const float velvet = noise_.nextVelvet(3400.0 + 2600.0 * velocity_, osRate_) * velvetEnv;
    const float skinBurst = clickFilter_.process(pink * 0.75f + velvet * 1.35f) * osScale * 1.45f;
    const float burstEnv = std::exp(-timeMs * 5.2f / duration) * attack;
    float modalMix = 0.72f;
    if (settings_.clickType == ClickType::noise) modalMix = 0.0f;
    if (settings_.clickType == ClickType::tick) modalMix = 0.35f;
    if (settings_.clickType == ClickType::analog) modalMix = 0.86f;
    return (modalSum * modalMix + skinBurst * burstEnv * (1.0f - modalMix)) * level * std::sqrt(velocity_);
}

float KickVoice::renderDrumLayer(float timeMs) noexcept {
    const float decayMs = finiteClamp(layer_.decayMs, 180.0f, 5.0f, 1500.0f);
    const float progress = std::clamp(timeMs / decayMs, 0.0f, 1.0f);
    const bool gatedSustain = settings_.ampEnabled && !reversing_;
    float decay = 1.0f;
    if (!gatedSustain) decay = shapedFall(progress, 0.15f);
    const float bright = 0.45f + 0.55f * velocity_;
    const float osNoiseGain = std::sqrt(static_cast<float>(std::max(1, decimator_.factor()))) * 2.2f;
    const float nyquist = static_cast<float>(osRate_ * 0.45);

    switch (layer_.sound) {
        case DrumSoundType::snare: {
            float wireEnv = 1.0f;
            if (!gatedSustain) wireEnv = std::exp(-timeMs * (4.2f + bright * 1.6f) / decayMs);
            const float headBend = 1.0f + 0.045f * std::exp(-timeMs / 1.6f);
            constexpr std::array<float, 4> ratios{ 1.0f, 1.518f, 1.835f, 2.292f };
            constexpr std::array<float, 4> gains{ 0.58f, 0.44f, 0.18f, 0.12f };
            constexpr std::array<float, 4> tausMs{ 16.0f, 13.5f, 6.5f, 4.8f };
            const float coupling = 0.06f * std::sin(static_cast<float>(modalPhases_[1]) * kTwoPi);
            float membrane = 0.0f;
            std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t mode) {
                const float modeHz = noteHz_ * ratios[mode] * headBend;
                if (modeHz >= nyquist) return;
                float modeEnv = 1.0f;
                if (!gatedSustain) modeEnv = std::exp(-timeMs / tausMs[mode]);
                const float phaseMod = mode == 0 ? coupling : 0.0f;
                membrane += gains[mode] * modeEnv * std::sin(static_cast<float>(modalPhases_[mode]) * kTwoPi + phaseMod);
                modalPhases_[mode] += static_cast<double>(modeHz) / osRate_;
                modalPhases_[mode] -= std::floor(modalPhases_[mode]);
            });

            const size_t combLen = std::clamp(static_cast<size_t>(osRate_ * 0.00135), size_t{ 8 }, snareComb_.size() - 1u);
            const size_t readPos = snareCombIndex_ % combLen;
            const float delayed = snareComb_[readPos];
            snareCombLp_ = 0.55f * delayed + 0.45f * snareCombLp_;
            const float rawNoise = noise_.nextPink() * 0.85f + noise_.nextVelvet(4200.0, osRate_) * 0.45f;
            const float rattleDrive = rawNoise * (1.0f + 0.28f * std::abs(membrane));
            snareComb_[readPos] = rattleDrive + 0.46f * snareCombLp_;
            snareCombIndex_ = readPos + 1u;
            const float wireRattle = clickFilter_.process(rattleDrive + 0.62f * snareCombLp_) * osNoiseGain;
            return decay * 0.72f * (membrane * 0.42f + wireRattle * wireEnv * (0.68f + bright * 0.18f));
        }
        case DrumSoundType::clap: {
            float burst = 0.0f;
            std::ranges::for_each(std::views::iota(0, 4), [&](int tap) {
                const float startMs = static_cast<float>(tap) * 6.0f;
                if (timeMs >= startMs) burst += std::exp(-(timeMs - startMs) * 1.1f);
            });
            const float clapNoise = clickFilter_.process(noise_.next()) * osNoiseGain;
            float diffuseTail = 1.0f;
            if (!gatedSustain) diffuseTail = std::exp(-timeMs * 5.0f / decayMs);
            const float texture = std::min(1.0f, burst * 0.48f) * 0.78f + diffuseTail * 0.26f;
            return clapNoise * texture * decay * (0.85f + bright * 0.25f);
        }
        case DrumSoundType::hat: {
            const float metalEnv = std::exp(-timeMs / 5.2f);
            constexpr std::array<float, 6> ratios{ 1.0f, 1.3420f, 1.6180f, 1.9275f, 2.5029f, 3.1847f };
            std::array<float, 6> squares{};
            const auto polyBlep = [](double t, double dt) noexcept {
                if (t < dt) {
                    const double x = t / dt;
                    return x + x - x * x - 1.0;
                }
                if (t > 1.0 - dt) {
                    const double x = (t - 1.0) / dt;
                    return x * x + x + x + 1.0;
                }
                return 0.0;
            };
            std::ranges::for_each(std::views::iota(size_t{ 0 }, ratios.size()), [&](size_t osc) {
                const double oscHz = static_cast<double>(noteHz_ * ratios[osc]);
                const double dt = oscHz / osRate_;
                if (dt >= 0.45) return;
                const double p = modalPhases_[osc];
                const double half = p >= 0.5 ? p - 0.5 : p + 0.5;
                const double raw = p < 0.5 ? 1.0 : -1.0;
                const double bandFade = std::clamp((0.45 - dt) / 0.08, 0.0, 1.0);
                squares[osc] = static_cast<float>((raw + polyBlep(p, dt) - polyBlep(half, dt)) * bandFade);
                modalPhases_[osc] += dt;
                modalPhases_[osc] -= std::floor(modalPhases_[osc]);
            });
            const float carriers = squares[0] * 0.50f + squares[1] * 0.28f + squares[2] * 0.18f;
            const float ringPairs = squares[0] * squares[1] * 0.54f
                + squares[2] * squares[3] * 0.34f
                + squares[4] * squares[5] * 0.22f;
            const float rawMetal = (carriers * 0.42f + ringPairs * 0.68f) * metalEnv;
            const float hpCutoff = std::clamp(noteHz_ * 0.68f, 300.0f, nyquist * 0.8f);
            const float hpAlpha = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * static_cast<double>(hpCutoff) / osRate_));
            snareCombLp_ += hpAlpha * (rawMetal - snareCombLp_);
            const float metal = rawMetal - snareCombLp_;
            const float hatNoise = clickFilter_.process(noise_.next()) * osNoiseGain * (0.85f + bright * 0.3f);
            return (hatNoise * 0.80f + metal * 0.28f) * decay;
        }
        case DrumSoundType::kick:
            break;
    }
    return 0.0f;
}

float KickVoice::renderOversampled() noexcept {
    const double timeMsDouble = 1000.0 * static_cast<double>(osIndex_) / osRate_;
    const double nextTimeMsDouble = 1000.0 * static_cast<double>(osIndex_ + 1) / osRate_;
    const float timeMs = static_cast<float>(timeMsDouble);
    float sample = 0.0f;
    if (layer_.sound == DrumSoundType::kick) {
        float envelope = amplitudeAt(timeMs, settings_);
        if (settings_.ampEnabled && !reversing_) envelope = ampLevel();
        const float diveOctaves = pitchOctavesAt(timeMs, settings_);
        const float nyquist = static_cast<float>(osRate_ * 0.45);
        float frequency = std::min(noteHz_, nyquist);
        if (diveOctaves > 0.0f) {
            frequency = std::min(noteHz_ * std::exp2(diveOctaves), nyquist);
            if (knockDb_ > 0.01f) {
                const float trackedKnock = std::min(knockHz_ * std::exp2(0.22f * std::min(diveOctaves, 2.0f)), nyquist);
                knock_.c = dsp::peakCoeffs(trackedKnock, 1.05, knockDb_, osRate_);
            }
        }
        if (diveOctaves <= 0.0f && osIndex_ > 0 && knockDb_ > 0.01f) {
            const float prevDive = pitchOctavesAt(static_cast<float>(1000.0 * static_cast<double>(osIndex_ - 1) / osRate_), settings_);
            if (prevDive > 0.0f) knock_.c = dsp::peakCoeffs(knockHz_, 1.05, knockDb_, osRate_);
        }

        const float bodyPhase = static_cast<float>(phase_) * kTwoPi;
        const float tone = finiteClamp(settings_.bodyTone, 0.65f, 0.0f, 1.0f);
        const float fundamental = std::sin(bodyPhase);
        float harm = 0.0f;
        if (tone > 0.0f && (timeMs < 80.0f || diveOctaves > 0.0f)) {
            const float harmEnv = std::exp(-timeMs / 8.0f);
            const float sweepBite = std::clamp(diveOctaves * 1.15f, 0.0f, 1.0f) * std::exp(-timeMs / 16.0f);
            harm = std::clamp(std::max(harmEnv, sweepBite), 0.0f, 1.0f) * tone;
        }
        float harmonics = 0.0f;
        if (harm > 1.0e-5f) {
            const auto partial = [&](float multiple) {
                const float harmHz = frequency * multiple;
                if (harmHz >= nyquist) return 0.0f;
                const float u = std::clamp((nyquist - harmHz) / (nyquist * 0.2f), 0.0f, 1.0f);
                const float fade = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * u);
                return std::sin(bodyPhase * multiple) * fade;
            };
            harmonics = harm * (0.46f * partial(2.0f) + 0.24f * partial(3.0f) + 0.12f * partial(4.0f));
        }
        float body = knock_.process(fundamental * (1.0f - harm * 0.2f) + harmonics);

        const float subLevel = finiteClamp(settings_.subLevel, 0.35f, 0.0f, 1.0f);
        const float blend = std::min(1.0f, 2.0f * diveOctaves);
        const float attackWeight = blend * blend * (3.0f - 2.0f * blend);
        const float attackSub = std::sin(static_cast<float>(subPhase_) * kTwoPi);
        const float subOctAmount = finiteClamp(settings_.subOctave, 0.0f, 0.0f, 1.0f);
        const float subOctSine = std::sin(static_cast<float>(subOctavePhase_) * kTwoPi);
        const float sub = subLevel * std::lerp(fundamental, attackSub, attackWeight) + subOctAmount * 0.38f * subOctSine;
        const float velocityScale = 0.65f + 0.35f * velocity_;
        const float bodyLinear = body * 0.45f * velocityScale;
        const float mixed = bodyLinear * (1.0f - subLevel * 0.45f) + sub * 0.42f;
        const float click = renderClick(timeMs) * 0.42f;
        const float punchAmount = finiteClamp(settings_.clickPunch, 0.0f, 0.0f, 1.0f);
        float punchGain = 1.0f;
        if (punchAmount > 0.0f && timeMs < 45.0f) punchGain += punchAmount * 0.55f * std::exp(-timeMs / 4.5f);
        const float preSat = (mixed + click) * punchGain * envelope;
        const float satDrive = 1.35f + 0.45f * punchAmount + 0.25f * tone;
        const float satBias = 0.16f + 0.08f * punchAmount;
        const float saturated = saturator_.process(preSat, satDrive, satBias) * 1.01f;
        sample = tone_.process(saturated);
        sample = lowCut_.process(sample);

        const double phaseStep = integratePitchCycles(timeMsDouble, nextTimeMsDouble, noteHz_, osRate_ * 0.45, settings_);
        phase_ += phaseStep;
        subPhase_ += phaseStep;
        subOctavePhase_ += 0.5 * phaseStep;
    }
    if (layer_.sound != DrumSoundType::kick) {
        sample = renderDrumLayer(timeMs);
        sample = tone_.process(sample);
        sample = lowCut_.process(sample);
        if (settings_.ampEnabled && !reversing_) sample *= ampLevel();
        const double step = static_cast<double>(noteHz_) / osRate_;
        phase_ += step;
        subPhase_ += step;
        subOctavePhase_ += 0.5 * step;
    }
    sample *= gain_;

    phase_ -= std::floor(phase_);
    subPhase_ -= std::floor(subPhase_);
    subOctavePhase_ -= std::floor(subOctavePhase_);
    ++osIndex_;
    ++ampIndex_;
    return std::isfinite(sample) ? sample : 0.0f;
}

float KickVoice::renderHostSample() noexcept {
    if (!active_ || hostIndex_ >= hostLength_) {
        active_ = false;
        return 0.0f;
    }
    float sample = 0.0f;
    if (decimator_.factor() == 1) sample = renderOversampled();
    if (decimator_.factor() > 1) {
        std::array<float, dsp::Decimator::kMaxFactor> samples{};
        int idx = 0;
        while (idx < decimator_.factor()) {
            samples[static_cast<size_t>(idx)] = renderOversampled();
            ++idx;
        }
        sample = decimator_.process(samples);
    }
    sample = layerFilter_.process(sample, layer_.highPassHz, layer_.lowPassHz, hostRate_);
    sample = declick(sample);
    sample = std::clamp(sample, -1.0f, 1.0f);
    if (chokeRemaining_ >= 0) {
        sample *= static_cast<float>(chokeRemaining_) / static_cast<float>(chokeLength_);
        if (--chokeRemaining_ <= 0) active_ = false;
    }
    previousSample_ = std::isfinite(sample) ? sample : 0.0f;
    ++hostIndex_;
    if (hostIndex_ >= hostLength_ || (settings_.ampEnabled && releaseStart_ >= 0 && ampIndex_ - releaseStart_ >= releaseLength_)) active_ = false;
    return previousSample_;
}

float KickVoice::declick(float sample) noexcept {
    if (declickLeft_ <= 0) return sample;
    const float w = static_cast<float>(declickLeft_) / static_cast<float>(declickLength_);
    const float fade = w * w * (3.0f - 2.0f * w);
    --declickLeft_;
    return sample * (1.0f - 0.55f * fade) + declickFrom_ * fade;
}

float KickVoice::nextSample() noexcept {
    if (!active_) return 0.0f;
    if (!reversing_) return renderHostSample();
    if (reverseSamplesRemaining_ <= 0 || reverseCacheLength_ <= 0) {
        active_ = false;
        reversing_ = false;
        return 0.0f;
    }

    const double clampedPos = std::clamp(reverseSourcePosition_, 0.0, static_cast<double>(reverseCacheLength_ - 1));
    const auto idx0 = static_cast<size_t>(clampedPos);
    const size_t idx1 = std::min(idx0 + 1u, static_cast<size_t>(reverseCacheLength_ - 1));
    const float frac = static_cast<float>(clampedPos - static_cast<double>(idx0));
    float sample = std::lerp(reverseCache_[idx0], reverseCache_[idx1], frac);

    const int playedSamples = std::max(0, reverseTotalSamples_ - reverseSamplesRemaining_);
    const int tailWindow = std::clamp(std::min(reverseTotalSamples_ / 10, static_cast<int>(hostRate_ * 0.012)), 1, std::max(1, reverseTotalSamples_ / 4));
    if (playedSamples < tailWindow) {
        const float u = static_cast<float>(playedSamples) / static_cast<float>(tailWindow);
        sample *= 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * u);
    }
    const int endWindow = std::clamp(std::min(reverseTotalSamples_ / 28, static_cast<int>(hostRate_ * 0.0025)), 1, std::max(1, reverseTotalSamples_ / 8));
    if (reverseSamplesRemaining_ <= endWindow) {
        const float u = static_cast<float>(reverseSamplesRemaining_) / static_cast<float>(endWindow);
        sample *= 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * u);
    }

    if (settings_.ampEnabled) sample *= ampLevel();
    ampIndex_ += decimator_.factor();
    sample = declick(sample);
    sample = std::clamp(sample, -1.0f, 1.0f);
    if (chokeRemaining_ >= 0) {
        sample *= static_cast<float>(chokeRemaining_) / static_cast<float>(chokeLength_);
        if (--chokeRemaining_ <= 0) active_ = false;
    }
    reverseSourcePosition_ = std::max(0.0, reverseSourcePosition_ - reverseSourceStep_);
    if (--reverseSamplesRemaining_ <= 0 || (settings_.ampEnabled && releaseStart_ >= 0 && ampIndex_ - releaseStart_ >= releaseLength_)) {
        active_ = false;
        reversing_ = false;
    }
    previousSample_ = std::isfinite(sample) ? sample : 0.0f;
    return previousSample_;
}

std::vector<float> renderKick(const KickSettings& settings, double sampleRate, int midiNote) {
    KickVoice voice;
    voice.prepare(sampleRate);
    auto preview = settings;
    preview.ampEnabled = false;
    voice.start(preview, midiNote, 1.0f);
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
    const bool anySolo = std::ranges::any_of(layers, [](const auto& layer) { return layer.enabled && layer.solo; });
    std::ranges::for_each(std::views::iota(size_t{ 0 }, voices.size()), [&](size_t index) {
        voices[index].prepare(sampleRate);
        const auto& layer = layers[index];
        if (!layer.enabled || layer.gain <= 0.0001f || (anySolo && !layer.solo)) return;
        if (midiNote < layer.keyLow || midiNote > layer.keyHigh || layer.velocityHigh < 127) return;
        auto preview = settings;
        preview.ampEnabled = false;
        voices[index].start(preview, midiNote, 1.0f, layer);
        float layerLength = finiteClamp(layer.decayMs, 180.0f, 5.0f, 1500.0f);
        if (layer.sound == DrumSoundType::kick)
            layerLength = hitLengthMs(settings) * finiteClamp(layer.decayMs, 300.0f, 5.0f, 1500.0f) / 300.0f;
        lengthMs = std::max(lengthMs, std::min(2000.0f, layerLength));
    });

    std::vector<float> samples;
    samples.reserve(static_cast<size_t>(std::ceil(sampleRate * lengthMs * 0.001f)));
    while (std::ranges::any_of(voices, [](const KickVoice& voice) { return voice.isActive(); })) {
        float sample = 0.0f;
        for (size_t index = 0; index < voices.size(); ++index) {
            const auto& layer = layers[index];
            const float polarity = layer.invertPolarity ? -1.0f : 1.0f;
            const float pan = finiteClamp(layer.pan, 0, -1, 1);
            sample += voices[index].nextSample() * polarity * (1.0f - std::abs(pan) * 0.5f);
        }
        samples.push_back(std::isfinite(sample) ? sample : 0.0f);
    }
    return samples;
}

} // namespace openedmkick::core
