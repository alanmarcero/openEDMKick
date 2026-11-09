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
constexpr float kClickAttackMs = 0.18f;
constexpr float kAntiClickReleaseMs = 3.5f;
constexpr float kClickDecayMs = 6.5f;
constexpr float kNoiseDecayMs = 2.4f;
constexpr float kPunchDecayMs = 24.0f;
constexpr float kCurveOctaveScale = 2.5f;
constexpr double kDcBlockHz = 30.0;

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

[[nodiscard]] float sanitize(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}

// Positive tail curves still decay faster than negative ones (v1 polarity).
[[nodiscard]] float tailEnvelopeGain(float progress, float tailCurve) noexcept {
    const float p = std::clamp(progress, 0.0f, 1.0f);
    if (p <= 0.0f) {
        return 1.0f;
    }
    if (p >= 1.0f) {
        return 0.0f;
    }
    const float k = std::exp2(1.35f + std::clamp(tailCurve, -1.0f, 1.0f) * 1.65f);
    const float end = std::exp(-k);
    const float denom = 1.0f - end;
    if (!(denom > 1.0e-8f)) {
        return 1.0f - p;
    }
    return std::clamp((std::exp(-k * p) - end) / denom, 0.0f, 1.0f);
}

[[nodiscard]] float nextDeterministicNoise(uint32_t& state) noexcept {
    state = state * 1664525u + 1013904223u;
    const float unit = static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
    return unit * 2.0f - 1.0f;
}

[[nodiscard]] float onePoleLow(float input, float& state, float cutoffHz, double sampleRate) noexcept {
    const float safeCutoff = std::isfinite(cutoffHz) ? cutoffHz : 1000.0f;
    const float limited = std::clamp(safeCutoff, 30.0f, static_cast<float>(sampleRate * 0.45));
    const float coefficient = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * static_cast<double>(limited) / sampleRate));
    state += coefficient * (input - state);
    if (!std::isfinite(state) || std::abs(state) > 8.0f) {
        state = 0.0f;
    } else if (std::abs(state) < 1.0e-20f) {
        state = 0.0f;
    }
    return state;
}

[[nodiscard]] float layerAttack(int sampleIndex, double sampleRate) noexcept {
    const float samples = std::max(2.0f, static_cast<float>(sampleRate * 0.00015));
    return std::min(1.0f, static_cast<float>(sampleIndex + 1) / samples);
}

[[nodiscard]] float renderSnareLayer(
    const KickSynthSettings& settings,
    double sampleRate,
    int sampleIndex,
    double timeMs,
    float pitchMultiplier,
    float releaseFade,
    KickOscillatorState& osc) noexcept {
    const float level = std::clamp(settings.snareLevel, 0.0f, 1.0f);
    if (level <= 0.0008f) {
        return 0.0f;
    }
    const float tone = std::clamp(settings.snareTone, 0.0f, 1.0f);
    const float decayMs = snareDecayMs(settings.snareDecay);
    const float bodyEnv = std::exp(-static_cast<float>(timeMs) / std::max(10.0f, decayMs * 0.78f));
    const float wireEnv = std::exp(-static_cast<float>(timeMs) / std::max(8.0f, decayMs * 0.40f));
    const float bodyHz = std::clamp(172.0f * pitchMultiplier * (0.9f + 0.22f * tone), 70.0f, 680.0f);
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    const float body = std::sin(static_cast<float>(osc.snarePhase) * twoPi);
    osc.snarePhase += static_cast<double>(bodyHz) / sampleRate;
    osc.snarePhase -= std::floor(osc.snarePhase);

    const float noise = nextDeterministicNoise(osc.snareNoise);
    const float lowCut = 220.0f + tone * 780.0f;
    const float highCut = std::clamp(850.0f * std::exp2(tone * 2.45f), lowCut + 200.0f, 9000.0f);
    const float brighter = onePoleLow(noise, osc.snareHigh, highCut, sampleRate);
    const float darker = onePoleLow(brighter, osc.snareLow, lowCut, sampleRate);
    const float wires = brighter - darker;
    const float attack = layerAttack(sampleIndex, sampleRate);
    const float shell = (1.0f - tone * 0.82f) * body * bodyEnv;
    const float rattle = (0.28f + 0.85f * tone) * wires * wireEnv;
    return sanitize((shell * 0.72f + rattle * 0.95f) * attack * releaseFade * level * 0.85f);
}

[[nodiscard]] float renderClapLayer(
    const KickSynthSettings& settings,
    double sampleRate,
    int sampleIndex,
    double timeMs,
    float releaseFade,
    KickOscillatorState& osc) noexcept {
    const float level = std::clamp(settings.clapLevel, 0.0f, 1.0f);
    if (level <= 0.0008f) {
        return 0.0f;
    }
    const float spread = std::clamp(settings.clapSpread, 0.0f, 1.0f);
    const float tone = std::clamp(settings.clapTone, 0.0f, 1.0f);
    const float decayScale = clapDecayScale(settings.clapDecay);
    const float gapMs = clapSpreadMs(spread);
    const float burstLimit = 22.0f * decayScale;
    float burst = 0.0f;
    std::ranges::for_each(std::views::iota(0, 4), [&](int hit) {
        const float startMs = static_cast<float>(hit) * gapMs;
        const float elapsed = static_cast<float>(timeMs) - startMs;
        if (elapsed >= 0.0f && elapsed < burstLimit) {
            const float hitGain = 1.0f - 0.12f * static_cast<float>(hit);
            const float burstTau = (2.6f + 0.22f * static_cast<float>(hit)) * decayScale;
            burst = std::max(burst, hitGain * std::exp(-elapsed / burstTau));
        }
    });
    const float tailStart = gapMs * 3.0f;
    const float tailTau = (16.0f + spread * 36.0f) * decayScale;
    const float tail = timeMs > tailStart
        ? 0.22f * std::exp(-(static_cast<float>(timeMs) - tailStart) / tailTau)
        : 0.0f;
    const float envelope = std::max(burst, tail) * layerAttack(sampleIndex, sampleRate);
    const float noise = nextDeterministicNoise(osc.clapNoise);
    const float highCut = 4200.0f + tone * 4200.0f;
    const float lowCut = 700.0f + tone * 5200.0f;
    const float brighter = onePoleLow(noise, osc.clapHigh, highCut, sampleRate);
    const float darker = onePoleLow(brighter, osc.clapLow, std::min(lowCut, highCut * 0.72f), sampleRate);
    const float crack = brighter - darker;
    return sanitize(crack * envelope * releaseFade * level * 1.55f);
}

[[nodiscard]] float renderHatLayer(
    const KickSynthSettings& settings,
    double sampleRate,
    int sampleIndex,
    double timeMs,
    float pitchMultiplier,
    float releaseFade,
    KickOscillatorState& osc) noexcept {
    const float level = std::clamp(settings.hatLevel, 0.0f, 1.0f);
    if (level <= 0.0008f) {
        return 0.0f;
    }
    const float tone = std::clamp(settings.hatTone, 0.0f, 1.0f);
    const float decayMs = hatDecayMs(settings.hatDecay);
    constexpr std::array<float, 6> kRatios{ 1.0f, 1.47f, 1.86f, 2.33f, 2.97f, 3.71f };
    constexpr std::array<float, 6> kAmps{ 0.42f, 0.78f, 1.0f, 0.66f, 0.52f, 0.34f };
    const float baseHz = std::clamp((560.0f + tone * 980.0f) * pitchMultiplier, 80.0f, static_cast<float>(sampleRate * 0.18));
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    float metal = 0.0f;
    std::ranges::for_each(std::views::iota(0, 6), [&](int partial) {
        const float hz = std::min(baseHz * kRatios[static_cast<size_t>(partial)], static_cast<float>(sampleRate * 0.45));
        metal += kAmps[static_cast<size_t>(partial)] * std::sin(static_cast<float>(osc.hatPhase[static_cast<size_t>(partial)]) * twoPi);
        osc.hatPhase[static_cast<size_t>(partial)] += static_cast<double>(hz) / sampleRate;
        osc.hatPhase[static_cast<size_t>(partial)] -= std::floor(osc.hatPhase[static_cast<size_t>(partial)]);
    });
    const float noise = nextDeterministicNoise(osc.hatNoise);
    const float chiffCut = 1600.0f + tone * 7000.0f;
    const float low = onePoleLow(noise, osc.hatLow, chiffCut, sampleRate);
    const float chiff = noise - low;
    const float metalEnv = std::exp(-static_cast<float>(timeMs) / std::max(8.0f, decayMs));
    const float noiseEnv = std::exp(-static_cast<float>(timeMs) / std::max(6.0f, decayMs * 0.48f));
    const float attack = layerAttack(sampleIndex, sampleRate);
    const float metallic = metal * 0.16f * metalEnv * (0.95f - 0.35f * tone);
    const float air = chiff * noiseEnv * (0.32f + 0.85f * tone);
    return sanitize((metallic + air) * attack * releaseFade * level * 0.72f);
}

[[nodiscard]] int kickLengthSamples(float tailMs, double sampleRate) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const float clampedTail = std::clamp(tailMs, kMinTailMs, kMaxTailMs);
    return std::max(32, static_cast<int>(std::round(safeRate * clampedTail * 0.001)));
}

[[nodiscard]] int layerHorizonSamples(const KickSynthSettings& settings, double sampleRate) noexcept {
    const float horizonMs = activeLayerHorizonMs(settings);
    if (horizonMs <= 0.0f) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::round(std::max(8000.0, sampleRate) * static_cast<double>(horizonMs) * 0.001)));
}

[[nodiscard]] float computeKickStep(
    const KickSynthSettings& settings,
    double sampleRate,
    int sampleIndex,
    int bodySamples,
    int totalSamples,
    float semitoneOffset,
    KickOscillatorState& osc) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const double timeMs = (static_cast<double>(sampleIndex) * 1000.0) / safeRate;
    const float pitchMultiplier = std::exp2(std::clamp(semitoneOffset, -48.0f, 48.0f) / 12.0f);
    const float baseHz = fundamentalHzForNote(settings.note, settings.octave, settings.fineCents) * pitchMultiplier;
    const float startHz = std::max(baseHz + 20.0f, settings.startPitchHz * pitchMultiplier);

    const float pitchDecayMs = std::max(2.0f, settings.pitchDecayMs);
    const float pitchProgress = std::clamp(static_cast<float>(timeMs) / pitchDecayMs, 0.0f, 1.0f);
    const float sweep = pitchSweepGain(pitchProgress, settings.pitchCurve);
    const float instantaneousHz = std::clamp(baseHz + (startHz - baseHz) * sweep, 8.0f, 18000.0f);

    const int safeBody = std::max(1, bodySamples);
    const float tailProgress = std::clamp(
        static_cast<float>(sampleIndex) / static_cast<float>(safeBody),
        0.0f,
        1.0f);
    const float ampDecay = tailEnvelopeGain(tailProgress, settings.tailCurve);
    const float attackSamples = std::max(4.0f, static_cast<float>(safeRate * kAttackRampMs * 0.001));
    const float attackEnv = std::min(1.0f, static_cast<float>(sampleIndex + 1) / attackSamples);
    const float clickAttackSamples = std::max(2.0f, static_cast<float>(safeRate * kClickAttackMs * 0.001));
    const float clickAttack = std::min(1.0f, static_cast<float>(sampleIndex + 1) / clickAttackSamples);
    const float releaseSamples = std::max(8.0f, static_cast<float>(safeRate * kAntiClickReleaseMs * 0.001));
    const float bodyRemaining = static_cast<float>(std::max(0, bodySamples - sampleIndex));
    const float bodyRelease = std::min(1.0f, bodyRemaining / releaseSamples);
    const float layerRemaining = static_cast<float>(std::max(0, totalSamples - sampleIndex));
    const float layerRelease = std::min(1.0f, layerRemaining / releaseSamples);

    const float punchAmount = std::clamp(settings.punch, 0.0f, 1.0f);
    const float punchEnv = std::exp(-static_cast<float>(timeMs) / kPunchDecayMs);
    const float bodyGain = sampleIndex >= bodySamples
        ? 0.0f
        : attackEnv * ampDecay * bodyRelease * (1.0f + 0.55f * punchAmount * punchEnv);

    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    const float bodyAngle = static_cast<float>(osc.bodyPhase) * twoPi;
    const float fundamental = std::sin(bodyAngle);
    // Octave helps the kick translate; the third adds beater knock. Both die
    // with the punch envelope so the sustain stays a tuned sine.
    const float octave = std::sin(bodyAngle * 2.0f) * (0.16f * punchAmount * punchEnv);
    const float thirdHarmonic = std::sin(bodyAngle * 3.0f) * (0.12f * punchAmount * punchEnv);
    const float rawBody = fundamental + octave + thirdHarmonic;

    const float driveAmount = std::clamp(settings.subDrive, 0.0f, 1.0f);
    const float driveGain = 1.0f + 4.5f * driveAmount;
    const float drivenNorm = std::tanh(driveGain);
    const float drivenBody = drivenNorm > 1.0e-6f ? std::tanh(rawBody * driveGain) / drivenNorm : rawBody;
    const float blendedBody = rawBody + driveAmount * (drivenBody - rawBody);

    const float clickEnv = std::exp(-static_cast<float>(timeMs) / kClickDecayMs);
    const float clickChirpHz = std::clamp(settings.clickFreqHz * pitchMultiplier, 200.0f, 16000.0f)
        * (0.5f + 0.5f * clickEnv);
    const float noise = nextDeterministicNoise(osc.noise);
    const float noiseEnv = std::exp(-static_cast<float>(timeMs) / kNoiseDecayMs);
    const float fastCrack = std::exp(-static_cast<float>(timeMs) / (kClickDecayMs * 0.35f));
    const float clickBurst = 0.65f * std::sin(static_cast<float>(osc.clickPhase) * twoPi)
                           + 0.20f * std::sin(static_cast<float>(osc.clickPhase) * 2.7f * twoPi) * fastCrack
                           + 0.15f * noise * noiseEnv;
    const float clickSignal = sampleIndex >= bodySamples
        ? 0.0f
        : std::clamp(settings.clickLevel, 0.0f, 1.0f) * clickEnv * clickAttack * clickBurst * 0.55f * bodyRelease;

    osc.bodyPhase += static_cast<double>(instantaneousHz) / safeRate;
    osc.bodyPhase -= std::floor(osc.bodyPhase);
    osc.clickPhase += static_cast<double>(clickChirpHz) / safeRate;
    osc.clickPhase -= std::floor(osc.clickPhase);

    // Layers are skipped entirely at zero so a dry kick stays sample-identical.
    const float layers = renderSnareLayer(settings, safeRate, sampleIndex, timeMs, pitchMultiplier, layerRelease, osc)
        + renderClapLayer(settings, safeRate, sampleIndex, timeMs, layerRelease, osc)
        + renderHatLayer(settings, safeRate, sampleIndex, timeMs, pitchMultiplier, layerRelease, osc);
    // Leave a little headroom so the pitch sweep is not flattened, then soft-clip.
    const float mixed = sanitize((blendedBody * bodyGain + clickSignal + sanitize(layers)) * 0.9f);
    const float soft = std::tanh(mixed) * 1.08f;
    const float dcCoeff = static_cast<float>(std::exp(-2.0 * std::numbers::pi * kDcBlockHz / safeRate));
    const float blocked = soft - osc.dcX + dcCoeff * osc.dcY;
    osc.dcX = soft;
    osc.dcY = std::abs(blocked) < 1.0e-20f ? 0.0f : blocked;
    if (!std::isfinite(osc.dcY)) {
        osc.dcX = 0.0f;
        osc.dcY = 0.0f;
        return 0.0f;
    }
    return std::clamp(osc.dcY, -1.0f, 1.0f);
}

void renderKickWaveformInto(
    std::vector<float>& buffer,
    const KickSynthSettings& settings,
    double sampleRate,
    float semitoneOffset) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const int bodySamples = kickLengthSamples(settings.tailLengthMs, safeRate);
    const int totalSamples = std::max(bodySamples, layerHorizonSamples(settings, safeRate));
    buffer.resize(static_cast<size_t>(totalSamples));
    KickOscillatorState osc;
    std::ranges::for_each(std::views::iota(0, totalSamples), [&](int i) {
        buffer[static_cast<size_t>(i)] = computeKickStep(
            settings, safeRate, i, bodySamples, totalSamples, semitoneOffset, osc);
    });
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

float pitchSweepGain(float progress, float pitchCurve) noexcept {
    const float p = std::clamp(progress, 0.0f, 1.0f);
    if (p <= 0.0f) {
        return 1.0f;
    }
    if (p >= 1.0f) {
        return 0.0f;
    }
    const float k = std::exp2(0.85f + std::clamp(pitchCurve, -1.0f, 1.0f) * 2.1f);
    const float end = std::exp(-k);
    const float denom = 1.0f - end;
    if (!(denom > 1.0e-8f)) {
        return 1.0f - p;
    }
    return std::clamp((std::exp(-k * p) - end) / denom, 0.0f, 1.0f);
}

std::vector<float> renderKickWaveform(
    const KickSynthSettings& settings,
    double sampleRate,
    float semitoneOffset) noexcept {
    std::vector<float> buffer;
    renderKickWaveformInto(buffer, settings, sampleRate, semitoneOffset);
    return buffer;
}

void encodeWavInto(std::vector<uint8_t>& bytes, const std::vector<float>& samples, uint32_t sampleRate) {
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(float));
    const uint32_t riffChunkSize = 36u + dataBytes;
    bytes.clear();
    bytes.reserve(std::max(bytes.capacity(), static_cast<size_t>(44u) + dataBytes));

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
}

std::vector<uint8_t> encodeWavBytes(const std::vector<float>& samples, uint32_t sampleRate) {
    std::vector<uint8_t> bytes;
    encodeWavInto(bytes, samples, sampleRate);
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

void KickWavCache::prepare(double sampleRate) {
    const size_t count = static_cast<size_t>(std::max(8000.0, sampleRate) * 2.6) + 8u;
    samples_.reserve(count);
    wavBytes_.reserve(44u + count * sizeof(float));
}

void KickWavCache::lockIn(const KickSynthSettings& settings, double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    renderKickWaveformInto(samples_, settings, sampleRate_, 0.0f);
    encodeWavInto(wavBytes_, samples_, static_cast<uint32_t>(std::round(sampleRate_)));
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

void KickVoice::prepare(double sampleRate) {
    const double safeRate = std::max(8000.0, sampleRate);
    const size_t count = static_cast<size_t>(safeRate * 2.6) + 8u;
    if (reverseBuffer_.capacity() < count) {
        reverseBuffer_.reserve(count);
    }
    updateDeclick(safeRate);
}

void KickVoice::updateDeclick(double sampleRate) noexcept {
    const double samples = std::max(8.0, std::max(8000.0, sampleRate) * 0.00045);
    declickCoeff_ = static_cast<float>(std::exp(-1.0 / samples));
}

void KickVoice::reset() noexcept {
    active_ = false;
    reversing_ = false;
    usingCache_ = false;
    sampleIndex_ = 0;
    bodySamples_ = 0;
    totalSamples_ = 0;
    osc_ = {};
    cacheReadPos_ = 0.0;
    cacheReadStep_ = 1.0;
    lastOutput_ = 0.0f;
    declick_ = 0.0f;
    reverseMusical_ = false;
    reverseBuffer_.clear();
}

void KickVoice::triggerForward(
    const KickSynthSettings& settings,
    const KickWavCache& cache,
    double sampleRate,
    float velocity,
    float semitoneOffset,
    float effectiveTailMs) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const float previous = active_ ? lastOutput_ : 0.0f;
    updateDeclick(safeRate);
    active_ = true;
    reversing_ = false;
    usingCache_ = cache.isLocked() && !cache.samples().empty();
    sampleIndex_ = 0;
    osc_ = {};
    declick_ = previous;
    velocity_ = std::clamp(velocity, 0.0f, 1.5f);
    semitoneOffset_ = semitoneOffset;
    overrideTailMs_ = effectiveTailMs;

    const float pitchRatio = std::exp2(semitoneOffset / 12.0f);
    cacheReadPos_ = 0.0;
    cacheReadStep_ = usingCache_ ? (cache.sampleRate() / safeRate) * static_cast<double>(pitchRatio) : 1.0;

    const float baseTailMs = usingCache_
        ? static_cast<float>((static_cast<double>(cache.samples().size()) * 1000.0) / std::max(1.0, cache.sampleRate() * static_cast<double>(pitchRatio)))
        : std::clamp(settings.tailLengthMs, kMinTailMs, kMaxTailMs);
    const bool shortenForRoll = effectiveTailMs > 0.0f;
    const float finalTailMs = shortenForRoll ? std::min(baseTailMs, std::max(kMinTailMs, effectiveTailMs)) : baseTailMs;
    bodySamples_ = std::max(32, static_cast<int>(std::round(safeRate * finalTailMs * 0.001)));
    const int layerSamples = (usingCache_ || shortenForRoll) ? 0 : layerHorizonSamples(settings, safeRate);
    totalSamples_ = std::max(bodySamples_, layerSamples);
}

void KickVoice::triggerReverse(
    const KickSynthSettings& settings,
    const KickWavCache& cache,
    double sampleRate,
    int durationSamples,
    float velocity,
    float semitoneOffset) noexcept {
    const double safeRate = std::max(8000.0, sampleRate);
    const float previous = active_ ? lastOutput_ : 0.0f;
    updateDeclick(safeRate);
    active_ = true;
    reversing_ = true;
    usingCache_ = cache.isLocked() && !cache.samples().empty();
    sampleIndex_ = 0;
    totalSamples_ = std::max(1, durationSamples);
    reverseMusical_ = false;
    declick_ = previous;
    velocity_ = std::clamp(velocity, 0.0f, 1.5f);
    semitoneOffset_ = semitoneOffset;

    if (!usingCache_) {
        const int bodySamples = kickLengthSamples(settings.tailLengthMs, safeRate);
        const size_t needed = static_cast<size_t>(std::max(bodySamples, layerHorizonSamples(settings, safeRate)));
        if (reverseBuffer_.capacity() < needed) {
            reverseBuffer_.reserve(needed);
        }
        renderKickWaveformInto(reverseBuffer_, settings, safeRate, semitoneOffset);
    }
}

void KickVoice::armMusicalReverse(double startPpq, double endPpq) noexcept {
    reverseStartPpq_ = startPpq;
    reverseEndPpq_ = endPpq;
    reverseMusical_ = reversing_ && endPpq > startPpq;
}

float KickVoice::synthesizeCurrentSample(const KickSynthSettings& settings, double sampleRate) noexcept {
    return computeKickStep(
        settings,
        sampleRate,
        sampleIndex_,
        bodySamples_ > 0 ? bodySamples_ : totalSamples_,
        totalSamples_,
        semitoneOffset_,
        osc_);
}

float KickVoice::nextSample(
    const KickSynthSettings& liveSettings,
    const KickWavCache& cache,
    double sampleRate,
    double currentPpq) noexcept {
    if (!active_) {
        return 0.0f;
    }
    const bool musicalReverse = reversing_ && reverseMusical_ && currentPpq >= 0.0;
    if (musicalReverse && currentPpq >= reverseEndPpq_) {
        active_ = false;
        lastOutput_ = 0.0f;
        declick_ = 0.0f;
        return 0.0f;
    }
    if (!musicalReverse && sampleIndex_ >= totalSamples_) {
        active_ = false;
        lastOutput_ = 0.0f;
        declick_ = 0.0f;
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
        const float progress = musicalReverse
            ? std::clamp(static_cast<float>((currentPpq - reverseStartPpq_) / std::max(1.0e-9, reverseEndPpq_ - reverseStartPpq_)), 0.0f, 1.0f)
            : static_cast<float>(sampleIndex_) / static_cast<float>(std::max(1, totalSamples_ - 1));
        const float reversePos = (1.0f - progress) * static_cast<float>(source.size() - 1u);
        const size_t idx0 = std::min(static_cast<size_t>( std::max(0.0f, reversePos) ), source.size() - 1u);
        const size_t idx1 = std::min(idx0 + 1u, source.size() - 1u);
        const float frac = reversePos - static_cast<float>(idx0);
        const float raw = source[idx0] + frac * (source[idx1] - source[idx0]);
        const float startFade = std::min(1.0f, static_cast<float>(sampleIndex_ + 1) / releaseSamples);
        const float edgeFade = musicalReverse
            ? std::min(1.0f, static_cast<float>((reverseEndPpq_ - currentPpq) / std::max(1.0e-6, (reverseEndPpq_ - reverseStartPpq_) * 0.04)))
            : std::min(1.0f, remaining / std::max(4.0f, releaseSamples * 0.4f));
        const float swellGain = progress * progress;
        ++sampleIndex_;
        if (!musicalReverse && sampleIndex_ >= totalSamples_) {
            active_ = false;
        }
        float shaped = sanitize(raw * swellGain * startFade * edgeFade * velocity_);
        shaped += declick_;
        declick_ *= declickCoeff_;
        if (std::abs(declick_) < 1.0e-6f) {
            declick_ = 0.0f;
        }
        if (!std::isfinite(shaped)) {
            shaped = 0.0f;
        }
        lastOutput_ = shaped;
        if (!active_) {
            lastOutput_ = 0.0f;
            declick_ = 0.0f;
        }
        return shaped;
    }

    // Live synthesis already fades the final samples. The cache waveform does
    // too, unless tail shortening stops playback before the cached file ends.
    float sampleValue = 0.0f;
    if (usingCache_) {
        const double samplesLeft = static_cast<double>(cache.samples().size()) - cacheReadPos_;
        const double samplesUntilStop = static_cast<double>(std::max(1, totalSamples_ - sampleIndex_));
        const bool truncated = samplesLeft > samplesUntilStop * std::max(0.0, cacheReadStep_) + 2.0;
        sampleValue = cache.sampleAt(cacheReadPos_) * (truncated ? endFade : 1.0f);
    } else {
        sampleValue = synthesizeCurrentSample(liveSettings, safeRate);
    }

    cacheReadPos_ += cacheReadStep_;
    ++sampleIndex_;
    if (sampleIndex_ >= totalSamples_) {
        active_ = false;
    }
    float shaped = sanitize(sampleValue * velocity_ + declick_);
    declick_ *= declickCoeff_;
    if (std::abs(declick_) < 1.0e-6f) {
        declick_ = 0.0f;
    }
    if (!std::isfinite(shaped)) {
        shaped = 0.0f;
    }
    lastOutput_ = shaped;
    if (!active_) {
        lastOutput_ = 0.0f;
        declick_ = 0.0f;
    }
    return shaped;
}

} // namespace openedmkick::core
