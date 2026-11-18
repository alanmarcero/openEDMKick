#include "Presets.h"

#include "AutoSequencer.h"
#include "KickSynth.h"
#include "Parameters.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <iterator>

namespace openedmkick::presets {
namespace {

using core::ClickType;

struct Voicing {
    ClickType click;
    float bodyTone, drive, knockHz, knockDb, clickLevel, clickMs, decayShape;
};

constexpr Voicing uplifting{ ClickType::beater, 0.49f, 0.11f, 142, 2.8f, 0.32f, 3.1f, -0.22f };
constexpr Voicing rolling{ ClickType::tick, 0.38f, 0.08f, 118, 1.8f, 0.24f, 2.0f, -0.32f };
constexpr Voicing progressive{ ClickType::analog, 0.24f, 0.07f, 108, 1.3f, 0.18f, 3.4f, -0.28f };
constexpr Voicing psy{ ClickType::tick, 0.32f, 0.09f, 135, 2.0f, 0.36f, 1.6f, -0.16f };
constexpr Voicing tech{ ClickType::noise, 0.58f, 0.15f, 168, 3.1f, 0.38f, 2.7f, -0.18f };
constexpr Voicing classic{ ClickType::analog, 0.43f, 0.12f, 152, 2.5f, 0.28f, 4.0f, -0.30f };

struct KickProfile {
    const char* name;
    const Voicing& voicing;
    float diveOctaves, diveMs, clickHz, holdMs, decayMs, sub;
};

constexpr std::array<KickProfile, 48> kProfiles{{
    { "Uplifting / First Light", uplifting, 2.4f, 33, 4600, 17, 151, 0.65f },
    { "Uplifting / Glass Beater", uplifting, 2.6f, 29, 6200, 14, 138, 0.60f },
    { "Uplifting / Wide Open", uplifting, 2.2f, 38, 3900, 21, 164, 0.70f },
    { "Uplifting / Downbeat", uplifting, 2.5f, 32, 5100, 20, 145, 0.63f },
    { "Uplifting / Top Line", uplifting, 2.8f, 27, 6700, 14, 131, 0.58f },
    { "Uplifting / Clean Lift", uplifting, 2.1f, 35, 4300, 19, 155, 0.68f },
    { "Uplifting / Short Arc", uplifting, 2.3f, 26, 5400, 13, 121, 0.64f },
    { "Uplifting / Main Room", uplifting, 2.7f, 34, 4800, 22, 160, 0.72f },
    { "Rolling / Pocket", rolling, 1.9f, 26, 3800, 13, 122, 0.74f },
    { "Rolling / Offbeat", rolling, 2.1f, 29, 4400, 16, 134, 0.70f },
    { "Rolling / Undertow", rolling, 1.7f, 31, 3200, 18, 143, 0.81f },
    { "Rolling / Needle", rolling, 2.3f, 22, 5600, 11, 113, 0.67f },
    { "Rolling / Low Rider", rolling, 1.8f, 28, 3500, 16, 139, 0.79f },
    { "Rolling / Slipstream", rolling, 2.2f, 25, 4700, 14, 128, 0.72f },
    { "Rolling / Bass Stitch", rolling, 1.6f, 24, 4100, 12, 116, 0.77f },
    { "Rolling / Late Shift", rolling, 2.0f, 32, 3700, 18, 146, 0.75f },
    { "Progressive / Soft Edge", progressive, 1.6f, 36, 2800, 20, 157, 0.77f },
    { "Progressive / Indigo", progressive, 1.8f, 39, 3100, 22, 166, 0.74f },
    { "Progressive / Closed Beater", progressive, 1.9f, 30, 2600, 15, 136, 0.69f },
    { "Progressive / Undercurrent", progressive, 1.5f, 34, 3000, 23, 162, 0.82f },
    { "Progressive / Blue Hour", progressive, 2.0f, 37, 3600, 18, 149, 0.72f },
    { "Progressive / Velvet Sub", progressive, 1.7f, 33, 2450, 21, 154, 0.80f },
    { "Progressive / Slow Rise", progressive, 1.8f, 42, 3200, 23, 168, 0.76f },
    { "Progressive / Muted Tap", progressive, 1.5f, 27, 3500, 13, 124, 0.73f },
    { "Psy / Triplet", psy, 2.7f, 21, 5600, 12, 112, 0.69f },
    { "Psy / Pinpoint", psy, 3.0f, 18, 7100, 10, 108, 0.66f },
    { "Psy / Elastic", psy, 2.4f, 31, 4800, 15, 131, 0.75f },
    { "Psy / Short Circuit", psy, 2.8f, 23, 6500, 11, 115, 0.70f },
    { "Psy / Ground Wire", psy, 2.2f, 26, 4600, 15, 125, 0.79f },
    { "Psy / Acid Tick", psy, 2.9f, 20, 7400, 13, 119, 0.68f },
    { "Psy / Full On", psy, 2.6f, 28, 6100, 16, 135, 0.76f },
    { "Psy / Microdose", psy, 2.3f, 19, 5200, 10, 105, 0.73f },
    { "Tech / Rivet", tech, 2.5f, 26, 5500, 15, 134, 0.64f },
    { "Tech / Dry Contact", tech, 2.3f, 23, 4600, 12, 115, 0.67f },
    { "Tech / Front Row", tech, 2.8f, 30, 6400, 18, 145, 0.61f },
    { "Tech / Machined", tech, 2.6f, 24, 6900, 14, 123, 0.63f },
    { "Tech / Dark Room", tech, 2.0f, 33, 3800, 19, 148, 0.73f },
    { "Tech / Knuckle", tech, 2.4f, 28, 4200, 16, 139, 0.68f },
    { "Tech / Staccato", tech, 2.9f, 21, 6100, 11, 110, 0.62f },
    { "Tech / Pressure Plate", tech, 2.2f, 32, 4950, 18, 142, 0.71f },
    { "Classic / Vinyl Tap", classic, 2.0f, 37, 3300, 18, 152, 0.68f },
    { "Classic / Nine O Nine", classic, 2.3f, 41, 3800, 20, 158, 0.65f },
    { "Classic / White Label", classic, 2.1f, 35, 4300, 17, 146, 0.69f },
    { "Classic / Gatefold", classic, 1.8f, 32, 2900, 16, 137, 0.74f },
    { "Classic / Warehouse", classic, 2.5f, 39, 4700, 21, 163, 0.67f },
    { "Classic / Afterhours", classic, 1.9f, 36, 3150, 20, 156, 0.76f },
    { "Classic / Dub Cut", classic, 1.7f, 29, 2700, 14, 128, 0.78f },
    { "Classic / Final Press", classic, 2.2f, 34, 4050, 17, 144, 0.71f }
}};

[[nodiscard]] FactoryPreset makeFactoryPreset(const KickProfile& kick) {
    const auto& voice = kick.voicing;
    return { kick.name, {
        { params::kSweepOctaves, kick.diveOctaves }, { params::kSweepMs, kick.diveMs },
        { params::kSweepShape, 0.16f }, { params::kSnapOctaves, kick.diveOctaves * 0.47f },
        { params::kSnapMs, voice.clickMs * 0.6f }, { params::kBodyTone, voice.bodyTone },
        { params::kBodyDrive, voice.drive }, { params::kSaturation, static_cast<float>(core::Saturation::warm) },
        { params::kKnockHz, voice.knockHz }, { params::kKnockDb, voice.knockDb },
        { params::kGrit, 0.03f }, { params::kToneHz, 8700 + kick.clickHz * 0.55f },
        { params::kSubLevel, kick.sub }, { params::kStartPhase, 0.18f },
        { params::kClickType, static_cast<float>(voice.click) }, { params::kClickLevel, voice.clickLevel },
        { params::kClickHz, kick.clickHz }, { params::kClickMs, voice.clickMs },
        { params::kAttackMs, 0.24f }, { params::kHoldMs, kick.holdMs },
        { params::kDecayMs, kick.decayMs }, { params::kDecayShape, voice.decayShape },
        { params::kLowCutHz, 20 }, { params::kClip, 0.035f },
        { params::kAmpEnabled, 0 }, { params::kAmpAttack, 0 }, { params::kAmpDecay, 100 },
        { params::kAmpSustain, 0.8f }, { params::kAmpRelease, 300 },
        { params::kSubOctave, 0.0f }, { params::kClickPunch, 0.0f }, { params::kStereoWidth, 0.0f }
    } };
}

} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> result;
        result.reserve(kProfiles.size());
        std::ranges::transform(kProfiles, std::back_inserter(result), makeFactoryPreset);
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
