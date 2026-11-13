#include "Presets.h"

#include "AutoSequencer.h"
#include "KickSynth.h"
#include "Parameters.h"

#include <algorithm>
#include <array>
#include <ranges>

namespace openedmkick::presets {
namespace {

struct KickProfile {
    const char* name;
    float sweepOctaves;
    float sweepMs;
    float bodyTone;
    float drive;
    float knockHz;
    float subLevel;
    float clickLevel;
    float clickHz;
    float decayMs;
};

constexpr std::array<KickProfile, 48> kProfiles{{
    { "Plasma Breaker", 2.7f, 64.0f, 0.78f, 0.72f, 190.0f, 0.34f, 0.82f, 6100.0f, 280.0f },
    { "Velvet Comet", 2.1f, 82.0f, 0.58f, 0.48f, 125.0f, 0.42f, 0.52f, 3600.0f, 430.0f },
    { "Circuit Bloom", 3.2f, 52.0f, 0.88f, 0.76f, 240.0f, 0.3f, 0.88f, 7200.0f, 255.0f },
    { "Titan Pulse", 2.4f, 74.0f, 0.69f, 0.65f, 155.0f, 0.52f, 0.68f, 4400.0f, 365.0f },
    { "Laser Anvil", 3.7f, 38.0f, 0.96f, 0.94f, 360.0f, 0.24f, 0.96f, 9400.0f, 185.0f },
    { "Ember Reactor", 2.5f, 88.0f, 0.72f, 0.62f, 205.0f, 0.47f, 0.58f, 3900.0f, 390.0f },
    { "Zero Gravity", 2.9f, 56.0f, 0.91f, 0.82f, 285.0f, 0.28f, 0.9f, 8100.0f, 230.0f },
    { "Giga Horizon", 1.9f, 110.0f, 0.52f, 0.55f, 100.0f, 0.61f, 0.42f, 2800.0f, 510.0f },
    { "Neon Forge", 3.4f, 47.0f, 0.94f, 0.88f, 320.0f, 0.26f, 0.94f, 8800.0f, 205.0f },
    { "Copper Thunder", 2.3f, 92.0f, 0.63f, 0.7f, 175.0f, 0.5f, 0.64f, 4200.0f, 410.0f },
    { "Static Cathedral", 2.8f, 68.0f, 0.84f, 0.79f, 260.0f, 0.35f, 0.78f, 5700.0f, 320.0f },
    { "Solar Torque", 3.6f, 41.0f, 0.98f, 0.96f, 410.0f, 0.22f, 1.0f, 10400.0f, 190.0f },
    { "Blacklight Driver", 2.6f, 77.0f, 0.76f, 0.68f, 145.0f, 0.43f, 0.72f, 4800.0f, 350.0f },
    { "Cherry Voltage", 3.1f, 58.0f, 0.9f, 0.86f, 275.0f, 0.31f, 0.86f, 7600.0f, 245.0f },
    { "Iron Halo", 2.2f, 98.0f, 0.61f, 0.58f, 118.0f, 0.57f, 0.48f, 3300.0f, 470.0f },
    { "Phase Hammer", 3.8f, 34.0f, 1.0f, 1.0f, 460.0f, 0.2f, 1.0f, 12000.0f, 165.0f },
    { "Lunar Pressure", 2.0f, 106.0f, 0.55f, 0.52f, 92.0f, 0.64f, 0.4f, 2600.0f, 540.0f },
    { "Violet Turbine", 3.0f, 62.0f, 0.86f, 0.8f, 230.0f, 0.36f, 0.8f, 6800.0f, 300.0f },
    { "Prism Furnace", 3.5f, 44.0f, 0.97f, 0.92f, 390.0f, 0.23f, 0.98f, 9900.0f, 195.0f },
    { "Deep Signal", 1.8f, 118.0f, 0.48f, 0.46f, 80.0f, 0.7f, 0.35f, 2200.0f, 590.0f },
    { "Orbit Crusher", 3.3f, 50.0f, 0.92f, 0.9f, 340.0f, 0.27f, 0.92f, 8500.0f, 220.0f },
    { "Chrome Mirage", 2.4f, 84.0f, 0.68f, 0.64f, 162.0f, 0.48f, 0.6f, 4000.0f, 400.0f },
    { "Amber Circuit", 2.7f, 71.0f, 0.81f, 0.74f, 212.0f, 0.39f, 0.75f, 5200.0f, 335.0f },
    { "Night Vector", 3.2f, 55.0f, 0.89f, 0.84f, 295.0f, 0.3f, 0.87f, 7400.0f, 260.0f },
    { "Redline Comet", 3.9f, 31.0f, 1.0f, 1.0f, 520.0f, 0.18f, 1.0f, 13500.0f, 150.0f },
    { "Soft Focus", 1.7f, 124.0f, 0.45f, 0.42f, 72.0f, 0.73f, 0.3f, 1800.0f, 620.0f },
    { "Eclipse Driver", 2.9f, 66.0f, 0.85f, 0.78f, 248.0f, 0.35f, 0.79f, 6300.0f, 315.0f },
    { "Rift Generator", 3.5f, 43.0f, 0.96f, 0.93f, 430.0f, 0.21f, 0.99f, 11200.0f, 180.0f },
    { "Fluoro Hammer", 3.1f, 57.0f, 0.91f, 0.87f, 305.0f, 0.29f, 0.9f, 7900.0f, 235.0f },
    { "Warm Engine", 2.1f, 103.0f, 0.57f, 0.51f, 108.0f, 0.63f, 0.44f, 2900.0f, 520.0f },
    { "Sonic Foundry", 3.6f, 39.0f, 0.99f, 0.98f, 475.0f, 0.2f, 1.0f, 12600.0f, 170.0f },
    { "Mirrored Impact", 2.5f, 80.0f, 0.74f, 0.69f, 185.0f, 0.45f, 0.7f, 4600.0f, 375.0f },
    { "Celestial Knuckle", 2.8f, 69.0f, 0.82f, 0.76f, 221.0f, 0.38f, 0.77f, 5600.0f, 325.0f },
    { "Voltage Orchard", 3.3f, 49.0f, 0.93f, 0.89f, 355.0f, 0.26f, 0.93f, 9000.0f, 215.0f },
    { "Hollow Planet", 1.9f, 114.0f, 0.5f, 0.48f, 86.0f, 0.68f, 0.38f, 2400.0f, 565.0f },
    { "Carbon Strobe", 3.7f, 36.0f, 0.98f, 0.97f, 495.0f, 0.19f, 1.0f, 13100.0f, 158.0f },
    { "Afterglow Driver", 2.4f, 90.0f, 0.66f, 0.63f, 150.0f, 0.54f, 0.55f, 3550.0f, 440.0f },
    { "Skyline Anvil", 3.0f, 61.0f, 0.87f, 0.83f, 268.0f, 0.33f, 0.83f, 7000.0f, 285.0f },
    { "Ultraviolet Tank", 3.4f, 46.0f, 0.95f, 0.91f, 380.0f, 0.24f, 0.96f, 9600.0f, 200.0f },
    { "Analog Meteor", 2.2f, 100.0f, 0.6f, 0.57f, 116.0f, 0.59f, 0.46f, 3150.0f, 485.0f },
    { "Mainframe Kick", 3.5f, 42.0f, 0.97f, 0.95f, 445.0f, 0.21f, 0.99f, 11600.0f, 175.0f },
    { "Scarlet Orbit", 2.6f, 76.0f, 0.77f, 0.71f, 198.0f, 0.42f, 0.72f, 4950.0f, 355.0f },
    { "Crystal Dynamo", 3.2f, 54.0f, 0.9f, 0.85f, 312.0f, 0.29f, 0.88f, 8250.0f, 250.0f },
    { "Substation", 1.8f, 120.0f, 0.47f, 0.44f, 76.0f, 0.75f, 0.32f, 1950.0f, 610.0f },
    { "Radiant Wrecking Ball", 3.8f, 33.0f, 1.0f, 1.0f, 540.0f, 0.18f, 1.0f, 14000.0f, 145.0f },
    { "Electric Rain", 2.7f, 73.0f, 0.8f, 0.73f, 207.0f, 0.4f, 0.76f, 5400.0f, 345.0f },
    { "Endless Horizon", 2.3f, 96.0f, 0.64f, 0.6f, 138.0f, 0.56f, 0.51f, 3400.0f, 455.0f },
    { "Eventide Engine", 3.4f, 45.0f, 0.94f, 0.9f, 365.0f, 0.25f, 0.95f, 9250.0f, 208.0f }
}};

[[nodiscard]] FactoryPreset makeFactoryPreset(const KickProfile& profile, int index) {
    using core::ClickType;
    using core::Saturation;
    const float snapOctaves = std::min(5.0f, profile.sweepOctaves * 0.72f);
    const float snapMs = std::clamp(profile.sweepMs * 0.045f, 0.5f, 8.0f);
    const float sweepShape = -0.65f + static_cast<float>((index * 7) % 15) * 0.11f;
    const float clickMs = std::clamp(2.2f + profile.clickLevel * 7.0f, 0.5f, 12.0f);
    const float holdMs = 20.0f + static_cast<float>((index * 5) % 8) * 7.0f;
    const float decayShape = -0.2f + static_cast<float>((index * 3) % 9) * 0.14f;
    const float lowCutHz = 18.0f + static_cast<float>(index % 7) * 3.0f;
    const float startPhase = 0.15f + static_cast<float>((index * 2) % 7) * 0.1f;
    const auto saturation = static_cast<Saturation>(index % 4);
    const auto clickType = static_cast<ClickType>((index / 2) % 4);
    const float knockDb = std::clamp(4.0f + profile.drive * 9.0f, 0.0f, 18.0f);
    const float grit = 0.18f + static_cast<float>(index % 6) * 0.13f;
    const float toneHz = 2600.0f + profile.bodyTone * 15000.0f;
    const float attackMs = 0.06f + static_cast<float>(index % 5) * 0.045f;
    const float clip = std::clamp(0.3f + profile.drive * 0.65f, 0.0f, 1.0f);
    return FactoryPreset{ profile.name, {
        { params::kSweepOctaves, profile.sweepOctaves }, { params::kSweepMs, profile.sweepMs },
        { params::kSweepShape, sweepShape }, { params::kSnapOctaves, snapOctaves },
        { params::kSnapMs, snapMs }, { params::kBodyTone, profile.bodyTone },
        { params::kBodyDrive, profile.drive }, { params::kSaturation, static_cast<float>(saturation) },
        { params::kKnockHz, profile.knockHz }, { params::kKnockDb, knockDb },
        { params::kGrit, grit }, { params::kToneHz, toneHz }, { params::kSubLevel, profile.subLevel },
        { params::kStartPhase, startPhase }, { params::kClickType, static_cast<float>(clickType) },
        { params::kClickLevel, profile.clickLevel }, { params::kClickHz, profile.clickHz },
        { params::kClickMs, clickMs }, { params::kAttackMs, attackMs }, { params::kHoldMs, holdMs },
        { params::kDecayMs, profile.decayMs }, { params::kDecayShape, decayShape },
        { params::kLowCutHz, lowCutHz }, { params::kClip, clip }
    } };
}

} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> result;
        result.reserve(kProfiles.size());
        std::ranges::for_each(std::views::iota(size_t{ 0 }, kProfiles.size()), [&](size_t i) {
            result.push_back(makeFactoryPreset(kProfiles[i], static_cast<int>(i)));
        });
        return result;
    }();
    return presets;
}

const std::vector<FactoryPreset>& fillBuildPresets() {
    static const std::array<float, core::kNumFillPresets> tailShorten{
        0.45f, 0.64f, 0.78f, 0.57f, 0.86f, 0.7f, 0.52f, 0.72f, 0.42f, 0.68f,
        0.82f, 0.9f, 0.74f, 0.6f, 0.55f, 0.77f, 0.84f, 0.88f, 0.66f, 0.91f,
        0.73f, 0.8f, 0.62f, 0.87f, 0.76f, 0.94f, 0.71f, 0.89f
    };
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> result;
        result.reserve(core::kNumFillPresets);
        std::ranges::for_each(std::views::iota(0, core::kNumFillPresets), [&](int index) {
            result.push_back(FactoryPreset{ core::kFillPresetNames[static_cast<size_t>(index)].data(), {
                { params::kAutoFillPreset, static_cast<float>(index) }, { params::kAutoGrid, 4.0f },
                { params::kAutoTailShorten, tailShorten[static_cast<size_t>(index)] },
                { params::kAutoCrushSweep, static_cast<float>((index * 3) % 8) / 20.0f }
            } });
        });
        return result;
    }();
    return presets;
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    std::ranges::for_each(preset.values, [&](const auto& value) {
        if (auto* parameter = state.getParameter(value.first))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value.second));
    });
}

void applyToCurrentAutoSlot(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    const int slot = std::clamp(static_cast<int>(state.getRawParameterValue(params::kAutoSlot)->load()),
                                0, core::kNumAutoSlots - 1);
    std::ranges::for_each(preset.values, [&](const auto& entry) {
        const bool perSlot = std::ranges::any_of(params::kAutoSlotIds, [&](const char* id) {
            return juce::String(id) == entry.first;
        });
        const auto paramId = perSlot ? params::autoSlotId(slot, entry.first) : entry.first;
        if (auto* parameter = state.getParameter(paramId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(entry.second));
    });
}

} // namespace openedmkick::presets
