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
