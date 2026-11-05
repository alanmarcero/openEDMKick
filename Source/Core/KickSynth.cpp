#include "KickSynth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <ranges>

namespace openedmkick::core {

namespace {

constexpr float kMinTailMs = 15.0f;
constexpr float kMaxTailMs = 2000.0f;
constexpr float kAttackRampMs = 1.2f;
constexpr float kAntiClickReleaseMs = 3.5f;
constexpr float kClickDecayMs = 6.5f;
constexpr float kPunchDecayMs = 24.0f;
constexpr float kCurveOctaveScale = 2.5f;

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xffu));
    out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
}

void appendTag(std::vector<uint8_t>& out, const char (&tag)[5]) {
    out.insert(out.end(), tag, tag + 4);
}

[[nodiscard]] uint32_t readU32(const uint8_t* ptr) noexcept {
    return static_cast<uint32_t>(ptr[0])
        | (static_cast<uint32_t>(ptr[1]) << 8u)
        | (static_cast<uint32_t>(ptr[2]) << 16u)
        | (static_cast<uint32_t>(ptr[3]) << 24u);
}

[[nodiscard]] uint16_t readU16(const uint8_t* ptr) noexcept {
    return static_cast<uint16_t>(ptr[0])
        | static_cast<uint16_t>(static_cast<uint16_t>(ptr[1]) << 8u);
}

[[nodiscard]] float computeKickStep(
    const KickSynthSettings& settings,
    double sampleRate,
    int sampleIndex,
    int totalSamples,
    float semitoneOffset,
    double& bodyPhase,
    double& clickPhase) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const double timeMs = (static_cast<double>(sampleIndex) * 1000.0) / safeRate;
    const float pitchMultiplier = std::exp2(semitoneOffset / 12.0f);
    const float baseHz = fundamentalHzForNote(settings.note, settings.octave, settings.fineCents) * pitchMultiplier;
    const float startHz = std::max(baseHz + 20.0f, settings.startPitchHz * pitchMultiplier);

    const float pitchDecayMs = std::max(2.0f, settings.pitchDecayMs);
    const float pitchProgress = std::clamp(static_cast<float>(timeMs) / pitchDecayMs, 0.0f, 1.0f);
    const float shapedPitch = 1.0f - shapeCurve(pitchProgress, settings.pitchCurve - 0.55f);
    const float instantaneousHz = baseHz + (startHz - baseHz) * shapedPitch * shapedPitch;

    const float tailProgress = std::clamp(
        static_cast<float>(sampleIndex) / static_cast<float>(std::max(1, totalSamples)),
        0.0f,
        1.0f);
    const float ampDecay = 1.0f - shapeCurve(tailProgress, settings.tailCurve);
    const float attackSamples = std::max(4.0f, static_cast<float>(safeRate * kAttackRampMs * 0.001));
    const float attackEnv = std::min(1.0f, static_cast<float>(sampleIndex + 1) / attackSamples);
    const float releaseSamples = std::max(8.0f, static_cast<float>(safeRate * kAntiClickReleaseMs * 0.001));
    const float remaining = static_cast<float>(std::max(0, totalSamples - sampleIndex));
    const float releaseFade = std::min(1.0f, remaining / releaseSamples);

    const float punchEnv = std::exp(-static_cast<float>(timeMs) / kPunchDecayMs);
    const float bodyGain = attackEnv * ampDecay * releaseFade * (1.0f + 0.55f * std::clamp(settings.punch, 0.0f, 1.0f) * punchEnv);

    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    const float fundamental = std::sin(static_cast<float>(bodyPhase) * twoPi);
    const float thirdHarmonic = std::sin(static_cast<float>(bodyPhase) * twoPi * 3.0f);
    const float rawBody = fundamental + 0.18f * std::clamp(settings.punch, 0.0f, 1.0f) * punchEnv * thirdHarmonic;

    const float driveAmount = std::clamp(settings.subDrive, 0.0f, 1.0f);
    const float driveGain = 1.0f + 4.5f * driveAmount;
    const float drivenBody = std::tanh(rawBody * driveGain) / std::tanh(driveGain);
    const float blendedBody = rawBody + driveAmount * (drivenBody - rawBody);

    const float clickEnv = std::exp(-static_cast<float>(timeMs) / kClickDecayMs);
    const float clickChirpHz = std::clamp(settings.clickFreqHz, 500.0f, 12000.0f) * (0.5f + 0.5f * clickEnv);
    const float clickBurst = std::sin(static_cast<float>(clickPhase) * twoPi)
        + 0.45f * std::sin(static_cast<float>(clickPhase) * twoPi * 2.37f);
    const float clickSignal = std::clamp(settings.clickLevel, 0.0f, 1.0f) * clickEnv * clickBurst * 0.55f;

    bodyPhase += static_cast<double>(instantaneousHz) / safeRate;
    bodyPhase -= std::floor(bodyPhase);
    clickPhase += static_cast<double>(clickChirpHz) / safeRate;
    clickPhase -= std::floor(clickPhase);

    return std::clamp(blendedBody * bodyGain + clickSignal * releaseFade, -1.0f, 1.0f);
}

} // namespace

float fundamentalHzForNote(int noteIndex, int octave, float fineCents) noexcept {
    const int wrappedNote = ((noteIndex % kNumNotes) + kNumNotes) % kNumNotes;
    const int clampedOctave = std::clamp(octave, 0, 3);
    const double semitonesFromA1 = static_cast<double>((clampedOctave - 1) * 12 + (wrappedNote - 9))
        + static_cast<double>(std::clamp(fineCents, -100.0f, 100.0f)) * 0.01;
    return static_cast<float>(55.0 * std::exp2(semitonesFromA1 / 12.0));
}

float shapeCurve(float progress, float curve) noexcept {
    const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
    if (clampedProgress <= 0.0f || clampedProgress >= 1.0f) {
        return clampedProgress;
    }
    const float clampedCurve = std::clamp(curve, -1.0f, 1.0f);
    const float exponent = std::exp2(-clampedCurve * kCurveOctaveScale);
    return std::pow(clampedProgress, exponent);
}

std::vector<float> renderKickWaveform(
    const KickSynthSettings& settings,
    double sampleRate,
    float semitoneOffset) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const float tailMs = std::clamp(settings.tailLengthMs, kMinTailMs, kMaxTailMs);
    const int totalSamples = std::max(32, static_cast<int>(std::round(safeRate * tailMs * 0.001)));
    std::vector<float> buffer(static_cast<size_t>(totalSamples), 0.0f);

    double bodyPhase = 0.0;
    double clickPhase = 0.0;
    std::ranges::for_each(std::views::iota(0, totalSamples), [&](int i) {
        buffer[static_cast<size_t>(i)] = computeKickStep(
            settings, safeRate, i, totalSamples, semitoneOffset, bodyPhase, clickPhase);
    });
    return buffer;
}

std::vector<uint8_t> encodeWavBytes(const std::vector<float>& samples, uint32_t sampleRate) {
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(float));
    const uint32_t riffChunkSize = 36u + dataBytes;
    std::vector<uint8_t> bytes;
    bytes.reserve(44u + dataBytes);

    appendTag(bytes, "RIFF");
    appendU32(bytes, riffChunkSize);
    appendTag(bytes, "WAVE");
    appendTag(bytes, "fmt ");
    appendU32(bytes, 16u);
    appendU16(bytes, 3u); // IEEE float format (3)
    appendU16(bytes, 1u); // Mono
    appendU32(bytes, sampleRate);
    appendU32(bytes, sampleRate * static_cast<uint32_t>(sizeof(float)));
    appendU16(bytes, static_cast<uint16_t>(sizeof(float)));
    appendU16(bytes, 32u);
    appendTag(bytes, "data");
    appendU32(bytes, dataBytes);

    const size_t headerSize = bytes.size();
    bytes.resize(headerSize + dataBytes);
    if (dataBytes > 0u) {
        std::memcpy(bytes.data() + headerSize, samples.data(), dataBytes);
    }
    return bytes;
}

bool decodeWavBytes(
    const std::vector<uint8_t>& wavBytes,
    std::vector<float>& outSamples,
    uint32_t& outSampleRate) {
    if (wavBytes.size() < 44u) {
        return false;
    }
    const bool validTags = std::memcmp(wavBytes.data(), "RIFF", 4) == 0
        && std::memcmp(wavBytes.data() + 8, "WAVE", 4) == 0
        && std::memcmp(wavBytes.data() + 12, "fmt ", 4) == 0
        && std::memcmp(wavBytes.data() + 36, "data", 4) == 0;
    if (!validTags) {
        return false;
    }
    const uint16_t audioFormat = readU16(wavBytes.data() + 20);
    const uint16_t channels = readU16(wavBytes.data() + 22);
    const uint32_t rate = readU32(wavBytes.data() + 24);
    const uint16_t bitsPerSample = readU16(wavBytes.data() + 34);
    const uint32_t dataBytes = readU32(wavBytes.data() + 40);
    if (audioFormat != 3u || channels != 1u || bitsPerSample != 32u || rate == 0u) {
        return false;
    }
    if (wavBytes.size() < 44u + static_cast<size_t>(dataBytes) || (dataBytes % sizeof(float)) != 0u) {
        return false;
    }
    const size_t count = dataBytes / sizeof(float);
    outSamples.resize(count);
    if (count > 0u) {
        std::memcpy(outSamples.data(), wavBytes.data() + 44, dataBytes);
    }
    outSampleRate = rate;
    return true;
}

void KickWavCache::lockIn(const KickSynthSettings& settings, double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    samples_ = renderKickWaveform(settings, sampleRate_, 0.0f);
    wavBytes_ = encodeWavBytes(samples_, static_cast<uint32_t>(std::round(sampleRate_)));
    locked_ = !samples_.empty();
}

void KickWavCache::unlock() noexcept {
    locked_ = false;
}

bool KickWavCache::loadFromWavBytes(const std::vector<uint8_t>& wavBytes) {
    std::vector<float> decoded;
    uint32_t rate = 48000u;
    if (!decodeWavBytes(wavBytes, decoded, rate) || decoded.empty()) {
        return false;
    }
    samples_ = std::move(decoded);
    sampleRate_ = static_cast<double>(rate);
    wavBytes_ = wavBytes;
    locked_ = true;
    return true;
}

float KickWavCache::sampleAt(double index) const noexcept {
    if (samples_.empty() || index < 0.0) {
        return 0.0f;
    }
    const size_t i0 = static_cast<size_t>(index);
    if (i0 >= samples_.size()) {
        return 0.0f;
    }
    const size_t i1 = std::min(i0 + 1u, samples_.size() - 1u);
    const float frac = static_cast<float>(index - static_cast<double>(i0));
    return samples_[i0] + frac * (samples_[i1] - samples_[i0]);
}

void KickVoice::reset() noexcept {
    active_ = false;
    reversing_ = false;
    usingCache_ = false;
    sampleIndex_ = 0;
    totalSamples_ = 0;
    bodyPhase_ = 0.0;
    clickPhase_ = 0.0;
    cacheReadPos_ = 0.0;
    cacheReadStep_ = 1.0;
}

void KickVoice::triggerForward(
    const KickSynthSettings& settings,
    const KickWavCache& cache,
    double sampleRate,
    float velocity,
    float semitoneOffset,
    float effectiveTailMs) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    active_ = true;
    reversing_ = false;
    usingCache_ = cache.isLocked() && !cache.samples().empty();
    sampleIndex_ = 0;
    bodyPhase_ = 0.0;
    clickPhase_ = 0.0;
    velocity_ = std::clamp(velocity, 0.0f, 1.5f);
    semitoneOffset_ = semitoneOffset;
    overrideTailMs_ = effectiveTailMs;

    const float pitchRatio = std::exp2(semitoneOffset / 12.0f);
    cacheReadPos_ = 0.0;
    cacheReadStep_ = usingCache_ ? (cache.sampleRate() / safeRate) * static_cast<double>(pitchRatio) : 1.0;

    const float baseTailMs = usingCache_
        ? static_cast<float>((static_cast<double>(cache.samples().size()) * 1000.0) / std::max(1.0, cache.sampleRate() * static_cast<double>(pitchRatio)))
        : std::clamp(settings.tailLengthMs, kMinTailMs, kMaxTailMs);
    const float finalTailMs = effectiveTailMs > 0.0f ? std::min(baseTailMs, std::max(kMinTailMs, effectiveTailMs)) : baseTailMs;
    totalSamples_ = std::max(32, static_cast<int>(std::round(safeRate * finalTailMs * 0.001)));
}

void KickVoice::triggerReverse(
    const KickSynthSettings& settings,
    const KickWavCache& cache,
    double sampleRate,
    int durationSamples,
    float velocity,
    float semitoneOffset) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    active_ = true;
    reversing_ = true;
    usingCache_ = cache.isLocked() && !cache.samples().empty();
    sampleIndex_ = 0;
    totalSamples_ = std::max(16, durationSamples);
    velocity_ = std::clamp(velocity, 0.0f, 1.5f);
    semitoneOffset_ = semitoneOffset;

    if (!usingCache_) {
        reverseBuffer_ = renderKickWaveform(settings, safeRate, semitoneOffset);
    }
}

float KickVoice::synthesizeCurrentSample(const KickSynthSettings& settings, double sampleRate) noexcept {
    return computeKickStep(
        settings,
        sampleRate,
        sampleIndex_,
        totalSamples_,
        semitoneOffset_,
        bodyPhase_,
        clickPhase_);
}

float KickVoice::nextSample(
    const KickSynthSettings& liveSettings,
    const KickWavCache& cache,
    double sampleRate) noexcept {
    if (!active_) {
        return 0.0f;
    }
    if (sampleIndex_ >= totalSamples_) {
        active_ = false;
        return 0.0f;
    }

    const double safeRate = std::max(8000.0, sampleRate);
    const float releaseSamples = std::max(8.0f, static_cast<float>(safeRate * kAntiClickReleaseMs * 0.001));
    const float remaining = static_cast<float>(std::max(0, totalSamples_ - sampleIndex_));
    const float endFade = std::min(1.0f, remaining / releaseSamples);

    if (reversing_) {
        const auto& source = (usingCache_ && !cache.samples().empty()) ? cache.samples() : reverseBuffer_;
        if (source.empty()) {
            active_ = false;
            return 0.0f;
        }
        const float progress = static_cast<float>(sampleIndex_) / static_cast<float>(std::max(1, totalSamples_ - 1));
        const float reversePos = (1.0f - progress) * static_cast<float>(source.size() - 1u);
        const size_t idx0 = std::min(static_cast<size_t>( std::max(0.0f, reversePos) ), source.size() - 1u);
        const size_t idx1 = std::min(idx0 + 1u, source.size() - 1u);
        const float frac = reversePos - static_cast<float>(idx0);
        const float raw = source[idx0] + frac * (source[idx1] - source[idx0]);
        const float startFade = std::min(1.0f, static_cast<float>(sampleIndex_ + 1) / releaseSamples);
        const float edgeFade = std::min(1.0f, remaining / std::max(4.0f, releaseSamples * 0.4f));
        const float swellGain = progress * progress;
        ++sampleIndex_;
        if (sampleIndex_ >= totalSamples_) {
            active_ = false;
        }
        return raw * swellGain * startFade * edgeFade * velocity_;
    }

    const float sampleValue = usingCache_
        ? cache.sampleAt(cacheReadPos_) * endFade
        : synthesizeCurrentSample(liveSettings, safeRate) * endFade;

    cacheReadPos_ += cacheReadStep_;
    ++sampleIndex_;
    if (sampleIndex_ >= totalSamples_) {
        active_ = false;
    }
    return sampleValue * velocity_;
}

} // namespace openedmkick::core
