#include "KickEngine.h"
#include "KickSynth.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("All 12 chromatic notes tune to exact equal-temperament fundamentals", "[synth]") {
    const float c1 = core::fundamentalHzForNote(0, 1, 0.0f);
    const float a1 = core::fundamentalHzForNote(9, 1, 0.0f);
    const float b1 = core::fundamentalHzForNote(11, 1, 0.0f);
    const float a2 = core::fundamentalHzForNote(9, 2, 0.0f);

    REQUIRE_THAT(a1, WithinAbs(55.0f, 0.001f));
    REQUIRE_THAT(a2, WithinAbs(110.0f, 0.001f));
    REQUIRE(c1 > 32.6f);
    REQUIRE(b1 < 61.8f);

    std::ranges::for_each(std::views::iota(0, core::kNumNotes - 1), [](int note) {
        const float lower = core::fundamentalHzForNote(note, 1, 0.0f);
        const float upper = core::fundamentalHzForNote(note + 1, 1, 0.0f);
        REQUIRE_THAT(upper / lower, WithinAbs(std::exp2(1.0f / 12.0f), 0.0001f));
    });
}

TEST_CASE("Locked-in WAV cache encodes valid RIFF WAV and isolates playback from synth edits", "[synth]") {
    const core::KickSynthSettings original{ 5, 1, 0.0f, 420.0f, 42.0f, 0.25f, 200.0f, 0.15f, 0.25f, 0.35f, 2800.0f, 0.45f };
    core::KickWavCache cache;
    cache.lockIn(original, 48000.0);

    REQUIRE(cache.isLocked());
    REQUIRE(cache.samples().size() == 9600u);
    REQUIRE(cache.wavBytes().size() == 44u + 9600u * sizeof(float));

    core::KickSynthSettings mutated = original;
    mutated.note = 11;
    mutated.startPitchHz = 1800.0f;
    mutated.tailLengthMs = 50.0f;
    mutated.clickLevel = 1.0f;

    core::KickVoice lockedVoice;
    lockedVoice.triggerForward(original, cache, 48000.0, 1.0f, 0.0f, -1.0f);
    REQUIRE(lockedVoice.isUsingCachedWav());

    const float firstLockedSample = lockedVoice.nextSample(mutated, cache, 48000.0);
    const float tenthLockedSample = [&] {
        float val = 0.0f;
        std::ranges::for_each(std::views::iota(0, 9), [&](int) {
            val = lockedVoice.nextSample(mutated, cache, 48000.0);
        });
        return val;
    }();

    REQUIRE_THAT(firstLockedSample, WithinAbs(cache.samples()[0], 0.0001f));
    REQUIRE_THAT(tenthLockedSample, WithinAbs(cache.samples()[9], 0.0001f));
}

TEST_CASE("Encoded WAV bytes decode back into identical kick samples", "[synth]") {
    const core::KickSynthSettings settings{};
    const auto rendered = core::renderKickWaveform(settings, 44100.0, 0.0f);
    const auto wavBytes = core::encodeWavBytes(rendered, 44100u);

    std::vector<float> decoded;
    uint32_t decodedRate = 0u;
    REQUIRE(core::decodeWavBytes(wavBytes, decoded, decodedRate));
    REQUIRE(decodedRate == 44100u);
    REQUIRE(decoded.size() == rendered.size());
    REQUIRE_THAT(decoded[100], WithinAbs(rendered[100], 1e-6f));
}

namespace {

[[nodiscard]] float estimateHz(const std::vector<float>& samples, double sampleRate, int start, int end) {
    const int i0 = std::clamp(start, 1, static_cast<int>(samples.size()) - 1);
    const int i1 = std::clamp(end, i0 + 1, static_cast<int>(samples.size()));
    std::vector<double> rising;
    std::ranges::for_each(std::views::iota(i0, i1), [&](int i) {
        const float previous = samples[static_cast<size_t>(i - 1)];
        const float current = samples[static_cast<size_t>(i)];
        if (previous <= 0.0f && current > 0.0f) {
            const float delta = current - previous;
            const float fraction = delta != 0.0f ? -previous / delta : 0.0f;
            rising.push_back((static_cast<double>(i - 1) + static_cast<double>(fraction)) / sampleRate);
        }
    });
    if (rising.size() < 3u) {
        return 0.0f;
    }
    double periodSum = 0.0;
    int periods = 0;
    std::ranges::for_each(std::views::iota(size_t{ 1 }, rising.size()), [&](size_t i) {
        periodSum += rising[i] - rising[i - 1u];
        ++periods;
    });
    return periods > 0 ? static_cast<float>(static_cast<double>(periods) / periodSum) : 0.0f;
}

[[nodiscard]] int countCrossings(const std::vector<float>& samples, int start, int end) {
    int crossings = 0;
    std::ranges::for_each(std::views::iota(start + 1, end), [&](int i) {
        const float previous = samples[static_cast<size_t>(i - 1)];
        const float current = samples[static_cast<size_t>(i)];
        if ((previous <= 0.0f && current > 0.0f) || (previous >= 0.0f && current < 0.0f)) {
            ++crossings;
        }
    });
    return crossings;
}

void writeWav(const std::string& path, const std::vector<float>& samples, uint32_t rate) {
    const auto bytes = core::encodeWavBytes(samples, rate);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

[[nodiscard]] double windowEnergy(const std::vector<float>& samples, int start, int end) {
    const int i0 = std::clamp(start, 0, static_cast<int>(samples.size()));
    const int i1 = std::clamp(end, i0, static_cast<int>(samples.size()));
    return std::accumulate(samples.begin() + i0, samples.begin() + i1, 0.0, [](double acc, float sample) {
        return acc + std::abs(static_cast<double>(sample));
    });
}

[[nodiscard]] double residualEnergy(const std::vector<float>& wet, const std::vector<float>& dry, int start, int end) {
    const int count = static_cast<int>(std::min(wet.size(), dry.size()));
    const int i0 = std::clamp(start, 0, count);
    const int i1 = std::clamp(end, i0, count);
    double energy = 0.0;
    std::ranges::for_each(std::views::iota(i0, i1), [&](int i) {
        energy += std::abs(static_cast<double>(wet[static_cast<size_t>(i)] - dry[static_cast<size_t>(i)]));
    });
    return energy;
}

[[nodiscard]] double differenceEnergy(const std::vector<float>& samples, int start, int end) {
    const int i0 = std::clamp(start, 1, static_cast<int>(samples.size()));
    const int i1 = std::clamp(end, i0, static_cast<int>(samples.size()));
    double energy = 0.0;
    std::ranges::for_each(std::views::iota(i0, i1), [&](int i) {
        const double delta = static_cast<double>(samples[static_cast<size_t>(i)] - samples[static_cast<size_t>(i - 1)]);
        energy += delta * delta;
    });
    return energy;
}

void peakMatch(std::vector<float>& samples, float targetPeak) {
    float peak = 0.0f;
    std::ranges::for_each(samples, [&](float sample) { peak = std::max(peak, std::abs(sample)); });
    if (peak > 1.0e-6f) {
        const float gain = targetPeak / peak;
        std::ranges::for_each(samples, [&](float& sample) { sample *= gain; });
    }
}

} // namespace

TEST_CASE("Pitch sweep starts at the click and lands on the fundamental", "[synth]") {
    REQUIRE_THAT(core::pitchSweepGain(0.0f, 0.25f), WithinAbs(1.0f, 1.0e-6f));
    REQUIRE_THAT(core::pitchSweepGain(1.0f, -1.0f), WithinAbs(0.0f, 1.0e-6f));
    REQUIRE(core::pitchSweepGain(0.35f, 1.0f) < core::pitchSweepGain(0.35f, -1.0f));

    float previous = 1.0f;
    std::ranges::for_each(std::views::iota(1, 11), [&](int step) {
        const float gain = core::pitchSweepGain(static_cast<float>(step) / 10.0f, 0.25f);
        REQUIRE(gain <= previous + 1.0e-5f);
        previous = gain;
    });
}

TEST_CASE("Live voice matches the rendered kick and settles on the tuned note", "[synth]") {
    core::KickSynthSettings settings{};
    settings.tailLengthMs = 420.0f;
    settings.pitchDecayMs = 36.0f;
    constexpr double kRate = 48000.0;
    const auto rendered = core::renderKickWaveform(settings, kRate, 0.0f);
    const core::KickWavCache cache{};
    core::KickVoice voice;
    voice.triggerForward(settings, cache, kRate, 1.0f, 0.0f, -1.0f);

    std::ranges::for_each(std::views::iota(size_t{ 0 }, rendered.size()), [&](size_t i) {
        REQUIRE(voice.isActive());
        REQUIRE_THAT(voice.nextSample(settings, cache, kRate), WithinAbs(rendered[i], 1.0e-5f));
    });
    REQUIRE(!voice.isActive());

    const float target = core::fundamentalHzForNote(settings.note, settings.octave, settings.fineCents);
    const int tailStart = static_cast<int>(rendered.size() / 2);
    const int tailEnd = static_cast<int>(rendered.size() * 4 / 5);
    const float measured = estimateHz(rendered, kRate, tailStart, tailEnd);
    REQUIRE_THAT(measured, WithinAbs(target, 1.0f));

    const double mean = std::accumulate(rendered.begin(), rendered.end(), 0.0) / static_cast<double>(rendered.size());
    REQUIRE(std::abs(mean) < 0.02);
}

TEST_CASE("Positive tail and pitch curves still decay faster than negative curves", "[synth]") {
    core::KickSynthSettings fastTail{};
    fastTail.tailLengthMs = 320.0f;
    fastTail.tailCurve = 1.0f;
    fastTail.clickLevel = 0.0f;
    core::KickSynthSettings slowTail = fastTail;
    slowTail.tailCurve = -1.0f;
    const auto fast = core::renderKickWaveform(fastTail, 48000.0, 0.0f);
    const auto slow = core::renderKickWaveform(slowTail, 48000.0, 0.0f);
    const auto midEnergy = [](const std::vector<float>& buffer) {
        const size_t begin = buffer.size() / 3;
        const size_t end = buffer.size() * 2 / 3;
        return std::accumulate(buffer.begin() + static_cast<std::ptrdiff_t>(begin),
            buffer.begin() + static_cast<std::ptrdiff_t>(end), 0.0,
            [](double acc, float sample) { return acc + std::abs(sample); });
    };
    REQUIRE(midEnergy(fast) < midEnergy(slow) * 0.8);

    core::KickSynthSettings fastPitch{};
    fastPitch.startPitchHz = 900.0f;
    fastPitch.pitchDecayMs = 70.0f;
    fastPitch.pitchCurve = 1.0f;
    fastPitch.tailLengthMs = 180.0f;
    fastPitch.clickLevel = 0.0f;
    fastPitch.punch = 0.0f;
    core::KickSynthSettings slowPitch = fastPitch;
    slowPitch.pitchCurve = -1.0f;
    const auto fastSweep = core::renderKickWaveform(fastPitch, 48000.0, 0.0f);
    const auto slowSweep = core::renderKickWaveform(slowPitch, 48000.0, 0.0f);
    const int windowStart = static_cast<int>(0.012 * 48000.0);
    const int windowEnd = static_cast<int>(0.030 * 48000.0);
    REQUIRE(countCrossings(fastSweep, windowStart, windowEnd) < countCrossings(slowSweep, windowStart, windowEnd));
}

TEST_CASE("Kick synthesis stays finite at parameter and sample-rate extremes", "[synth]") {
    const std::array<double, 4> rates{ 8000.0, 44100.0, 96000.0, 192000.0 };
    core::KickSynthSettings hot{};
    hot.note = 0;
    hot.octave = 0;
    hot.fineCents = -50.0f;
    hot.startPitchHz = 2500.0f;
    hot.pitchDecayMs = 5.0f;
    hot.pitchCurve = 1.0f;
    hot.tailLengthMs = 80.0f;
    hot.tailCurve = -1.0f;
    hot.subDrive = 1.0f;
    hot.clickLevel = 1.0f;
    hot.clickFreqHz = 10000.0f;
    hot.punch = 1.0f;

    core::KickSynthSettings longTail = hot;
    longTail.octave = 2;
    longTail.note = 11;
    longTail.fineCents = 50.0f;
    longTail.pitchDecayMs = 250.0f;
    longTail.pitchCurve = -1.0f;
    longTail.tailLengthMs = 1200.0f;
    longTail.tailCurve = 1.0f;
    longTail.clickFreqHz = 500.0f;

    std::ranges::for_each(rates, [&](double rate) {
        const auto buffer = core::renderKickWaveform(rate > 96000.0 ? hot : (rate < 16000.0 ? hot : longTail), rate, 12.0f);
        REQUIRE(!buffer.empty());
        std::ranges::for_each(buffer, [](float sample) {
            REQUIRE(std::isfinite(sample));
            REQUIRE(sample <= 1.0f);
            REQUIRE(sample >= -1.0f);
        });
    });
}

TEST_CASE("Retrigger declicks instead of stepping from the previous sample", "[synth]") {
    const core::KickSynthSettings settings{};
    const core::KickWavCache cache{};
    constexpr double kRate = 48000.0;
    core::KickVoice voice;
    voice.triggerForward(settings, cache, kRate, 1.0f, 0.0f, -1.0f);
    float previous = 0.0f;
    std::ranges::for_each(std::views::iota(0, 220), [&](int) {
        previous = voice.nextSample(settings, cache, kRate);
    });
    REQUIRE(std::abs(previous) > 0.05f);
    voice.triggerForward(settings, cache, kRate, 1.0f, 0.0f, -1.0f);
    const float first = voice.nextSample(settings, cache, kRate);
    REQUIRE(std::isfinite(first));
    REQUIRE(std::abs(first - previous) < 0.2f);
}

TEST_CASE("Locked WAV playback follows the host sample rate without rebuilding the hit", "[synth]") {
    core::KickSynthSettings settings{};
    settings.tailLengthMs = 240.0f;
    settings.pitchDecayMs = 30.0f;
    core::KickWavCache cache;
    cache.lockIn(settings, 48000.0);
    const float target = core::fundamentalHzForNote(settings.note, settings.octave, 0.0f);

    core::KickVoice voice;
    voice.prepare(96000.0);
    voice.triggerForward(settings, cache, 96000.0, 1.0f, 0.0f, -1.0f);
    std::vector<float> played;
    played.reserve(static_cast<size_t>(96000.0 * 0.3));
    while (voice.isActive()) {
        played.push_back(voice.nextSample(settings, cache, 96000.0));
    }
    REQUIRE(played.size() > cache.samples().size());
    const int tailStart = static_cast<int>(played.size() / 2);
    const int tailEnd = static_cast<int>(played.size() * 4 / 5);
    REQUIRE_THAT(estimateHz(played, 96000.0, tailStart, tailEnd), WithinAbs(target, 1.0f));
}

TEST_CASE("Rendered kick examples and objective metrics", "[synth][metrics]") {
    constexpr double kRate = 48000.0;
    core::KickSynthSettings club{};
    club.tailLengthMs = 420.0f;
    const auto clubHit = core::renderKickWaveform(club, kRate, 0.0f);
    const float target = core::fundamentalHzForNote(club.note, club.octave, club.fineCents);
    const float measured = estimateHz(clubHit, kRate, static_cast<int>(clubHit.size() / 2),
        static_cast<int>(clubHit.size() * 4 / 5));
    const double mean = std::accumulate(clubHit.begin(), clubHit.end(), 0.0) / static_cast<double>(clubHit.size());
    const float peak = std::ranges::max(clubHit, {}, [](float sample) { return std::abs(sample); });
    WARN("club_samples=" << clubHit.size() << " tuned_hz=" << target << " measured_hz=" << measured
         << " dc=" << mean << " peak=" << peak);

    core::KickSynthSettings pitched = club;
    const auto fifth = core::renderKickWaveform(pitched, kRate, 7.0f);
    const float fifthTarget = target * std::exp2(7.0f / 12.0f);
    const float fifthMeasured = estimateHz(fifth, kRate, static_cast<int>(fifth.size() / 2),
        static_cast<int>(fifth.size() * 4 / 5));
    WARN("fifth_target_hz=" << fifthTarget << " fifth_measured_hz=" << fifthMeasured);
    REQUIRE_THAT(fifthMeasured, WithinAbs(fifthTarget, 1.0f));

    if (const char* directory = std::getenv("OEK_RENDER_DIR")) {
        const std::string root(directory);
        writeWav(root + "/club-kick-f1.wav", clubHit, 48000u);
        writeWav(root + "/club-kick-f1-plus7st.wav", fifth, 48000u);
        core::KickSynthSettings punch = club;
        punch.punch = 1.0f;
        punch.clickLevel = 0.8f;
        punch.subDrive = 0.7f;
        punch.tailLengthMs = 260.0f;
        writeWav(root + "/punch-click.wav", core::renderKickWaveform(punch, kRate, 0.0f), 48000u);
    }
}

TEST_CASE("Timbre layers stay silent at zero and remain distinct when blended", "[synth][layers]") {
    core::KickSynthSettings dry{};
    dry.tailLengthMs = 420.0f;
    core::KickSynthSettings modifiersOnly = dry;
    modifiersOnly.snareTone = 1.0f;
    modifiersOnly.snareDecay = 1.0f;
    modifiersOnly.clapSpread = 1.0f;
    modifiersOnly.clapTone = 0.0f;
    modifiersOnly.hatTone = 0.0f;
    modifiersOnly.hatDecay = 1.0f;
    const auto dryHit = core::renderKickWaveform(dry, 48000.0, 0.0f);
    const auto stillDry = core::renderKickWaveform(modifiersOnly, 48000.0, 0.0f);
    REQUIRE(dryHit.size() == stillDry.size());
    std::ranges::for_each(std::views::iota(size_t{ 0 }, dryHit.size()), [&](size_t i) {
        REQUIRE_THAT(stillDry[i], WithinAbs(dryHit[i], 1.0e-6f));
    });

    auto withLayer = dry;
    withLayer.snareLevel = 0.85f;
    withLayer.snareTone = 0.7f;
    withLayer.snareDecay = 0.55f;
    const auto snareHit = core::renderKickWaveform(withLayer, 48000.0, 0.0f);
    REQUIRE(residualEnergy(snareHit, dryHit, 0, 4000) > 40.0);

    core::KickSynthSettings shortSnare = withLayer;
    shortSnare.snareDecay = 0.05f;
    core::KickSynthSettings longSnare = withLayer;
    longSnare.snareDecay = 0.95f;
    const auto shortHit = core::renderKickWaveform(shortSnare, 48000.0, 0.0f);
    const auto longHit = core::renderKickWaveform(longSnare, 48000.0, 0.0f);
    const int lateStart = static_cast<int>(0.09 * 48000.0);
    const int lateEnd = static_cast<int>(0.16 * 48000.0);
    REQUIRE(residualEnergy(longHit, dryHit, lateStart, lateEnd)
        > residualEnergy(shortHit, dryHit, lateStart, lateEnd) * 1.4);

    core::KickSynthSettings tight = dry;
    tight.clapLevel = 0.9f;
    tight.clapSpread = 0.0f;
    tight.clapTone = 0.7f;
    core::KickSynthSettings wide = tight;
    wide.clapSpread = 1.0f;
    const auto tightHit = core::renderKickWaveform(tight, 48000.0, 0.0f);
    const auto wideHit = core::renderKickWaveform(wide, 48000.0, 0.0f);
    const int flamStart = static_cast<int>(0.044 * 48000.0);
    const int flamEnd = static_cast<int>(0.058 * 48000.0);
    REQUIRE(residualEnergy(wideHit, dryHit, flamStart, flamEnd)
        > residualEnergy(tightHit, dryHit, flamStart, flamEnd) * 1.3);
    REQUIRE(residualEnergy(tightHit, dryHit, 0, 1500) > 20.0);

    core::KickSynthSettings hat = dry;
    hat.hatLevel = 0.85f;
    hat.hatTone = 0.85f;
    hat.hatDecay = 0.35f;
    const auto hatHit = core::renderKickWaveform(hat, 48000.0, 0.0f);
    const auto hatUp = core::renderKickWaveform(hat, 48000.0, 12.0f);
    const auto dryUp = core::renderKickWaveform(dry, 48000.0, 12.0f);
    REQUIRE(differenceEnergy(hatHit, 1, 1200) > differenceEnergy(snareHit, 1, 1200) * 1.15);
    REQUIRE(residualEnergy(hatUp, dryUp, 0, 1500) > residualEnergy(hatHit, dryHit, 0, 1500) * 0.4);
    const int hatWindow = static_cast<int>(0.012 * 48000.0);
    REQUIRE(differenceEnergy(hatUp, 1, hatWindow) > differenceEnergy(hatHit, 1, hatWindow));

    const auto again = core::renderKickWaveform(withLayer, 44100.0, 3.0f);
    const auto same = core::renderKickWaveform(withLayer, 44100.0, 3.0f);
    REQUIRE(again == same);

    core::KickSynthSettings extreme = dry;
    extreme.snareLevel = 1.0f;
    extreme.snareTone = 1.0f;
    extreme.snareDecay = 1.0f;
    extreme.clapLevel = 1.0f;
    extreme.clapSpread = 1.0f;
    extreme.clapTone = 1.0f;
    extreme.hatLevel = 1.0f;
    extreme.hatTone = 1.0f;
    extreme.hatDecay = 1.0f;
    extreme.tailLengthMs = 80.0f;
    std::ranges::for_each(std::array{ 8000.0, 44100.0, 96000.0, 192000.0 }, [&](double rate) {
        const auto buffer = core::renderKickWaveform(extreme, rate, -12.0f);
        std::ranges::for_each(buffer, [](float sample) {
            REQUIRE(std::isfinite(sample));
            REQUIRE(sample <= 1.0f);
            REQUIRE(sample >= -1.0f);
        });
    });

    core::KickWavCache cache;
    cache.lockIn(withLayer, 48000.0);
    core::KickVoice voice;
    voice.triggerForward(withLayer, cache, 48000.0, 1.0f, 0.0f, -1.0f);
    REQUIRE_THAT(voice.nextSample(dry, cache, 48000.0), WithinAbs(cache.samples()[0], 1.0e-5f));
    REQUIRE(std::abs(cache.samples()[200] - dryHit[200]) > 0.01f);
}

TEST_CASE("Layer audition examples are deterministic renders", "[synth][layers][metrics]") {
    constexpr double kRate = 48000.0;
    core::KickSynthSettings dry{};
    dry.tailLengthMs = 420.0f;
    core::KickSynthSettings snare = dry;
    snare.snareLevel = 0.8f;
    snare.snareTone = 0.7f;
    snare.snareDecay = 0.45f;
    core::KickSynthSettings clap = dry;
    clap.clapLevel = 0.75f;
    clap.clapSpread = 0.6f;
    clap.clapTone = 0.65f;
    core::KickSynthSettings hat = dry;
    hat.hatLevel = 0.7f;
    hat.hatTone = 0.8f;
    hat.hatDecay = 0.28f;
    core::KickSynthSettings stacked = dry;
    stacked.snareLevel = 0.55f;
    stacked.snareTone = 0.62f;
    stacked.snareDecay = 0.4f;
    stacked.clapLevel = 0.42f;
    stacked.clapSpread = 0.55f;
    stacked.clapTone = 0.7f;
    stacked.hatLevel = 0.38f;
    stacked.hatTone = 0.72f;
    stacked.hatDecay = 0.22f;

    auto dryHit = core::renderKickWaveform(dry, kRate, 0.0f);
    auto snareHit = core::renderKickWaveform(snare, kRate, 0.0f);
    auto clapHit = core::renderKickWaveform(clap, kRate, 0.0f);
    auto hatHit = core::renderKickWaveform(hat, kRate, 0.0f);
    auto stackedHit = core::renderKickWaveform(stacked, kRate, 0.0f);
    WARN("layer_rms dry=" << windowEnergy(dryHit, 0, static_cast<int>(dryHit.size()))
         << " snare=" << residualEnergy(snareHit, dryHit, 0, static_cast<int>(dryHit.size()))
         << " clap=" << residualEnergy(clapHit, dryHit, 0, static_cast<int>(dryHit.size()))
         << " hat=" << residualEnergy(hatHit, dryHit, 0, static_cast<int>(dryHit.size()))
         << " stacked=" << residualEnergy(stackedHit, dryHit, 0, static_cast<int>(dryHit.size())));

    if (const char* directory = std::getenv("OEK_RENDER_DIR")) {
        constexpr float kMatchPeak = 0.89f;
        peakMatch(dryHit, kMatchPeak);
        peakMatch(snareHit, kMatchPeak);
        peakMatch(clapHit, kMatchPeak);
        peakMatch(hatHit, kMatchPeak);
        peakMatch(stackedHit, kMatchPeak);
        const std::string root(directory);
        writeWav(root + "/dry-kick.wav", dryHit, 48000u);
        writeWav(root + "/kick-plus-snare.wav", snareHit, 48000u);
        writeWav(root + "/kick-plus-clap.wav", clapHit, 48000u);
        writeWav(root + "/kick-plus-hat.wav", hatHit, 48000u);
        writeWav(root + "/kick-plus-layers.wav", stackedHit, 48000u);

        const int gap = static_cast<int>(0.08 * kRate);
        std::vector<float> comparison;
        const std::array<const std::vector<float>*, 5> order{ &dryHit, &snareHit, &clapHit, &hatHit, &stackedHit };
        std::ranges::for_each(order, [&](const std::vector<float>* hit) {
            comparison.insert(comparison.end(), hit->begin(), hit->end());
            comparison.insert(comparison.end(), static_cast<size_t>(gap), 0.0f);
        });
        writeWav(root + "/layers-comparison.wav", comparison, 48000u);
    }
}

TEST_CASE("Layer decays stay compatible and can outlast a short kick tail", "[synth][layers][decay]") {
    REQUIRE_THAT(core::snareDecayMs(0.0f), WithinAbs(28.0f, 0.001f));
    REQUIRE_THAT(core::snareDecayMs(1.0f), WithinAbs(238.0f, 0.001f));
    REQUIRE_THAT(core::hatDecayMs(0.0f), WithinAbs(16.0f, 0.001f));
    REQUIRE_THAT(core::hatDecayMs(1.0f), WithinAbs(356.0f, 0.001f));
    REQUIRE_THAT(core::clapDecayScale(0.35f), WithinAbs(1.0f, 1.0e-6f));
    REQUIRE(core::clapDecayScale(0.0f) < 0.5f);
    REQUIRE(core::clapDecayScale(1.0f) > 5.0f);
    REQUIRE(core::activeLayerHorizonMs(core::KickSynthSettings{}) == 0.0f);

    constexpr double kRate = 48000.0;
    core::KickSynthSettings body{};
    body.tailLengthMs = 80.0f;
    body.clickLevel = 0.2f;

    core::KickSynthSettings shortHat = body;
    shortHat.hatLevel = 1.0f;
    shortHat.hatDecay = 0.0f;
    core::KickSynthSettings longHat = shortHat;
    longHat.hatDecay = 1.0f;
    const auto shortHatHit = core::renderKickWaveform(shortHat, kRate, 0.0f);
    const auto longHatHit = core::renderKickWaveform(longHat, kRate, 0.0f);
    REQUIRE(longHatHit.size() > 100000u);
    REQUIRE(shortHatHit.size() < 8000u);
    REQUIRE(windowEnergy(longHatHit, 4500, 9000) > windowEnergy(shortHatHit, 4500, 9000) * 4.0);

    core::KickSynthSettings shortSnare = body;
    shortSnare.snareLevel = 1.0f;
    shortSnare.snareTone = 0.3f;
    shortSnare.snareDecay = 0.0f;
    core::KickSynthSettings longSnare = shortSnare;
    longSnare.snareDecay = 1.0f;
    const auto shortSnareHit = core::renderKickWaveform(shortSnare, kRate, 0.0f);
    const auto longSnareHit = core::renderKickWaveform(longSnare, kRate, 0.0f);
    REQUIRE(longSnareHit.size() > shortSnareHit.size() * 3u);
    REQUIRE(windowEnergy(longSnareHit, 5000, 12000) > windowEnergy(shortSnareHit, 5000, 12000) * 3.0);

    core::KickSynthSettings shortClap = body;
    shortClap.clapLevel = 1.0f;
    shortClap.clapSpread = 0.45f;
    shortClap.clapDecay = 0.0f;
    core::KickSynthSettings legacyClap = shortClap;
    legacyClap.clapDecay = 0.35f;
    core::KickSynthSettings longClap = shortClap;
    longClap.clapDecay = 1.0f;
    const auto shortClapHit = core::renderKickWaveform(shortClap, kRate, 0.0f);
    const auto legacyClapHit = core::renderKickWaveform(legacyClap, kRate, 0.0f);
    const auto longClapHit = core::renderKickWaveform(longClap, kRate, 0.0f);
    REQUIRE(longClapHit.size() > legacyClapHit.size());
    REQUIRE(legacyClapHit.size() >= shortClapHit.size());
    REQUIRE(windowEnergy(longClapHit, 6000, 14000) > windowEnergy(shortClapHit, 6000, 14000) * 3.0);

    const auto voiceLength = [](const core::KickSynthSettings& settings, float velocity, float effectiveTailMs) {
        core::KickWavCache cache;
        core::KickVoice voice;
        voice.triggerForward(settings, cache, kRate, velocity, 0.0f, effectiveTailMs);
        int count = 0;
        double energy = 0.0;
        while (voice.isActive() && count < 200000) {
            const float sample = voice.nextSample(settings, cache, kRate);
            if (!std::isfinite(sample) || sample > 1.0f || sample < -1.0f) {
                return std::pair<int, double>{ -1, 0.0 };
            }
            energy += std::abs(static_cast<double>(sample));
            ++count;
        }
        return std::pair<int, double>{ count, energy };
    };
    const auto full = voiceLength(longHat, 1.0f, -1.0f);
    const auto half = voiceLength(longHat, 0.5f, -1.0f);
    REQUIRE(full.first == static_cast<int>(longHatHit.size()));
    REQUIRE(full.second > half.second * 1.8);
    REQUIRE(full.second < half.second * 2.2);
    const auto shortened = voiceLength(longHat, 1.0f, 40.0f);
    REQUIRE(shortened.first == 1920);

    core::KickWavCache locked;
    locked.lockIn(longHat, kRate);
    REQUIRE(locked.samples().size() == longHatHit.size());
    REQUIRE(windowEnergy(locked.samples(), 8000, 16000) > 1.0);
    core::KickVoice lockedVoice;
    lockedVoice.triggerForward(longHat, locked, kRate, 0.5f, 0.0f, -1.0f);
    REQUIRE(lockedVoice.isUsingCachedWav());
    REQUIRE_THAT(lockedVoice.nextSample(longHat, locked, kRate), WithinAbs(locked.samples()[0] * 0.5f, 1.0e-4f));

    core::KickWavCache empty;
    core::KickVoice reverse;
    reverse.triggerReverse(longClap, empty, kRate, 8000, 1.0f, 0.0f);
    double reverseEnergy = 0.0;
    for (int i = 0; i < 8000; ++i) {
        REQUIRE(reverse.isActive());
        const float sample = reverse.nextSample(longClap, empty, kRate);
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) <= 1.0f);
        reverseEnergy += std::abs(static_cast<double>(sample));
    }
    REQUIRE(!reverse.isActive());
    REQUIRE(reverseEnergy > 1.0);

    core::KickEngine engine;
    engine.prepare(kRate);
    core::EngineSettings settings{};
    settings.synth = longHat;
    settings.synth.snareLevel = 1.0f;
    settings.synth.snareDecay = 1.0f;
    settings.synth.clapLevel = 1.0f;
    settings.synth.clapDecay = 1.0f;
    settings.effects.drive = 1.0f;
    settings.effects.limiterEnabled = true;
    settings.effects.ceilingGain = 0.5f;
    std::vector<float> limited(24000, 0.0f);
    float* pointer = limited.data();
    const std::array<core::MidiTrigger, 1> trigger{ { { 0, 36, 1.0f, false } } };
    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    engine.process(settings, transport, trigger, &pointer, 1, static_cast<int>(limited.size()));
    REQUIRE(std::ranges::all_of(limited, [](float sample) {
        return std::isfinite(sample) && std::abs(sample) <= 0.501f;
    }));
    REQUIRE(windowEnergy(limited, 0, static_cast<int>(limited.size())) > 1.0);

    if (const char* directory = std::getenv("OEK_RENDER_DIR")) {
        const std::string root(directory);
        writeWav(root + "/hat-decay-short.wav", shortHatHit, 48000u);
        writeWav(root + "/hat-decay-long.wav", longHatHit, 48000u);
        writeWav(root + "/snare-decay-short.wav", shortSnareHit, 48000u);
        writeWav(root + "/snare-decay-long.wav", longSnareHit, 48000u);
        writeWav(root + "/clap-decay-short.wav", shortClapHit, 48000u);
        writeWav(root + "/clap-decay-legacy.wav", legacyClapHit, 48000u);
        writeWav(root + "/clap-decay-long.wav", longClapHit, 48000u);
        const int gap = static_cast<int>(0.08 * kRate);
        std::vector<float> comparison;
        for (const auto* hit : { &shortSnareHit, &longSnareHit, &shortClapHit, &longClapHit, &shortHatHit, &longHatHit }) {
            comparison.insert(comparison.end(), hit->begin(), hit->end());
            comparison.insert(comparison.end(), static_cast<size_t>(gap), 0.0f);
        }
        writeWav(root + "/decay-comparison.wav", comparison, 48000u);

        core::KickSynthSettings init{};
        core::KickSynthSettings subFloor{};
        subFloor.note = 5;
        subFloor.octave = 0;
        subFloor.startPitchHz = 180.0f;
        subFloor.pitchDecayMs = 90.0f;
        subFloor.tailLengthMs = 720.0f;
        subFloor.tailCurve = 0.05f;
        subFloor.clickLevel = 0.08f;
        subFloor.punch = 0.2f;
        subFloor.subDrive = 0.15f;
        core::KickSynthSettings clicky = init;
        clicky.note = 0;
        clicky.octave = 2;
        clicky.startPitchHz = 1400.0f;
        clicky.pitchDecayMs = 18.0f;
        clicky.tailLengthMs = 140.0f;
        clicky.clickLevel = 0.9f;
        clicky.clickFreqHz = 6200.0f;
        clicky.punch = 0.85f;
        clicky.subDrive = 0.1f;
        auto initHit = core::renderKickWaveform(init, kRate, 0.0f);
        auto subHit = core::renderKickWaveform(subFloor, kRate, 0.0f);
        auto clickHit = core::renderKickWaveform(clicky, kRate, 0.0f);
        core::KickSynthSettings openHat = longHat;
        openHat.note = 9;
        openHat.punch = 0.6f;
        auto openHatHit = core::renderKickWaveform(openHat, kRate, 0.0f);
        auto roomClapHit = longClapHit;
        core::KickSynthSettings stack{};
        stack.note = 10;
        stack.tailLengthMs = 260.0f;
        stack.punch = 0.55f;
        stack.snareLevel = 0.7f;
        stack.snareTone = 0.4f;
        stack.snareDecay = 0.85f;
        stack.clapLevel = 0.62f;
        stack.clapSpread = 0.4f;
        stack.clapTone = 0.75f;
        stack.clapDecay = 0.8f;
        stack.hatLevel = 0.55f;
        stack.hatTone = 0.6f;
        stack.hatDecay = 0.7f;
        auto stackHit = core::renderKickWaveform(stack, kRate, 0.0f);
        writeWav(root + "/preset-init.wav", initHit, 48000u);
        writeWav(root + "/preset-sub-floor.wav", subHit, 48000u);
        writeWav(root + "/preset-clicky-c2.wav", clickHit, 48000u);
        writeWav(root + "/preset-open-hat.wav", openHatHit, 48000u);
        writeWav(root + "/preset-long-clap.wav", roomClapHit, 48000u);
        writeWav(root + "/preset-full-stack.wav", stackHit, 48000u);
        std::vector<float> presetComparison;
        for (const auto* hit : { &initHit, &subHit, &clickHit, &openHatHit, &roomClapHit, &stackHit }) {
            presetComparison.insert(presetComparison.end(), hit->begin(), hit->end());
            presetComparison.insert(presetComparison.end(), static_cast<size_t>(gap), 0.0f);
        }
        writeWav(root + "/preset-comparison.wav", presetComparison, 48000u);
    }
}
