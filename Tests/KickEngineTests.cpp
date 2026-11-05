#include "KickEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numeric>
#include <ranges>

using Catch::Matchers::WithinAbs;
namespace core = openedmkick::core;

TEST_CASE("Regular mode triggers kick on any MIDI note and ignores Auto Mode Tail Shortener", "[engine]") {
    core::KickEngine engineA;
    core::KickEngine engineB;
    engineA.prepare(48000.0);
    engineB.prepare(48000.0);

    core::EngineSettings settingsShortTail{};
    settingsShortTail.autoEnabled = false;
    settingsShortTail.synth.tailLengthMs = 450.0f;
    settingsShortTail.autoSlots[0].tailShorten = 0.0f;

    core::EngineSettings settingsMaxShorten = settingsShortTail;
    settingsMaxShorten.autoSlots[0].tailShorten = 1.0f;

    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    const std::array<core::MidiTrigger, 1> triggerLow{ { { 0, 24, 1.0f } } };
    const std::array<core::MidiTrigger, 1> triggerHigh{ { { 0, 96, 1.0f } } };

    std::vector<float> outA(12000, 0.0f);
    std::vector<float> outB(12000, 0.0f);
    float* ptrA = outA.data();
    float* ptrB = outB.data();

    engineA.process(settingsShortTail, transport, triggerLow, &ptrA, 1, 12000);
    engineB.process(settingsMaxShorten, transport, triggerHigh, &ptrB, 1, 12000);

    const float tailEnergyA = std::accumulate(outA.begin() + 6000, outA.end(), 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });
    REQUIRE(tailEnergyA > 50.0f);

    std::ranges::for_each(std::views::iota(size_t{ 0 }, outA.size()), [&](size_t i) {
        REQUIRE_THAT(outA[i], WithinAbs(outB[i], 1e-6f));
    });
}

TEST_CASE("Auto mode tail shortener reduces tail energy on rapid 1/64 rolls", "[engine]") {
    core::KickEngine unshortenedEngine;
    core::KickEngine shortenedEngine;
    unshortenedEngine.prepare(48000.0);
    shortenedEngine.prepare(48000.0);

    unshortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1); // Mainstage 64th Machine Gun
    shortenedEngine.sequencer().loadPresetIntoSlot(0, 11, 1);

    core::EngineSettings unshortened{};
    unshortened.autoEnabled = true;
    unshortened.synth.tailLengthMs = 500.0f;
    unshortened.effects.limiterEnabled = false;
    unshortened.autoSlots[0].bars = 1;
    unshortened.autoSlots[0].fillPreset = 11;
    unshortened.autoSlots[0].tailShorten = 0.0f;

    core::EngineSettings shortened = unshortened;
    shortened.autoSlots[0].tailShorten = 0.95f;

    const core::Transport transport{ 128.0, 0.0, 4.0, true };
    std::vector<float> bufLong(8000, 0.0f);
    std::vector<float> bufShort(8000, 0.0f);
    float* pLong = bufLong.data();
    float* pShort = bufShort.data();

    unshortenedEngine.process(unshortened, transport, {}, &pLong, 1, 8000);
    shortenedEngine.process(shortened, transport, {}, &pShort, 1, 8000);

    const float lateLong = std::accumulate(bufLong.begin() + 3000, bufLong.begin() + 5500, 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });
    const float lateShort = std::accumulate(bufShort.begin() + 3000, bufShort.begin() + 5500, 0.0f,
        [](float acc, float s) { return acc + std::abs(s); });

    REQUIRE(lateShort < lateLong * 0.55f);
}
