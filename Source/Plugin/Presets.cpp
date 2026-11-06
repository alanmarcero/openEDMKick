#include "Presets.h"
#include "Parameters.h"
#include <algorithm>
#include <ranges>

namespace openedmkick::presets {

namespace {

FactoryPreset makeMultiSlotBuildPreset(
    const char* name,
    float noteIndex,
    float tailMs,
    float barChoice,
    float basePresetIdx,
    float tailShorten,
    float pitchRise,
    float crushRamp,
    float drive = 0.25f) {
    FactoryPreset preset{ name, {
        { params::kNote, noteIndex },
        { params::kTailLength, tailMs },
        { params::kDrive, drive },
        { params::kAutoEnabled, 1.0f },
        { params::kCrushRate, 5200.0f },
        { params::kCrushBits, 6.0f }
    } };
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const float slotFloat = static_cast<float>(slot);
        const float slotPreset = static_cast<float>((static_cast<int>(basePresetIdx) + slot) % core::kNumFillPresets);
        const std::array<std::pair<const char*, float>, 6> slotValues{ {
            { params::kAutoBars, std::clamp(barChoice + (slot >= 2 ? 1.0f : 0.0f), 0.0f, 6.0f) },
            { params::kAutoFillPreset, slotPreset },
            { params::kAutoGrid, 4.0f },
            { params::kAutoTailShorten, std::clamp(tailShorten + 0.06f * slotFloat, 0.2f, 0.95f) },
            { params::kAutoPitchRise, std::clamp(pitchRise + 2.0f * slotFloat, 0.0f, 12.0f) },
            { params::kAutoCrushSweep, std::clamp(crushRamp * (0.4f + 0.2f * slotFloat), 0.0f, 1.0f) }
        } };
        std::ranges::for_each(slotValues, [&](const auto& kv) {
            preset.values.emplace_back(params::autoSlotId(slot, kv.first), kv.second);
        });
    });
    return preset;
}

} // namespace

// Notes: 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B
// Bars: 0=1 Bar, 1=2 Bars, 2=4 Bars, 3=8 Bars, 4=16 Bars, 5=32 Bars, 6=64 Bars
const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets{
        { "Init Club Kick (F1)", {} },
        { "Big Room Sub Cannon (G1)", {
            { params::kNote, 7.0f }, { params::kStartPitch, 480.0f }, { params::kPitchDecay, 48.0f },
            { params::kTailLength, 480.0f }, { params::kTailCurve, 0.35f }, { params::kPunch, 0.65f },
            { params::kDrive, 0.25f } } },
        { "Tech House Tight Punch (A1)", {
            { params::kNote, 9.0f }, { params::kStartPitch, 360.0f }, { params::kPitchDecay, 32.0f },
            { params::kTailLength, 240.0f }, { params::kTailCurve, -0.15f }, { params::kClickLevel, 0.45f },
            { params::kPunch, 0.55f } } },
        { "Locked-In Studio Master (F#1)", {
            { params::kNote, 6.0f }, { params::kStartPitch, 440.0f }, { params::kPitchDecay, 40.0f },
            { params::kTailLength, 360.0f }, { params::kPunch, 0.55f }, { params::kLockIn, 1.0f } } },
        { "Beat 4 Reverse Pickup (E1)", {
            { params::kNote, 4.0f }, { params::kTailLength, 410.0f }, { params::kReverseMode, 2.0f },
            { params::kReverseMix, 1.0f }, { params::kDrive, 0.22f } } },
        { "Full Beat Synced Reverse (D#1)", {
            { params::kNote, 3.0f }, { params::kTailLength, 450.0f }, { params::kReverseMode, 1.0f },
            { params::kReverseMix, 1.0f }, { params::kDrive, 0.28f } } },
        { "Bitcrushed Industrial Stomp (C1)", {
            { params::kNote, 0.0f }, { params::kStartPitch, 620.0f }, { params::kPitchDecay, 55.0f },
            { params::kSubDrive, 0.65f }, { params::kDrive, 0.55f }, { params::kCrushRate, 3800.0f },
            { params::kCrushBits, 5.0f }, { params::kCrushMix, 0.48f } } },
        { "Hardstyle Tok & Crunch (G#1)", {
            { params::kNote, 8.0f }, { params::kStartPitch, 920.0f }, { params::kPitchDecay, 64.0f },
            { params::kTailLength, 460.0f }, { params::kSubDrive, 0.8f }, { params::kClickLevel, 0.7f },
            { params::kDrive, 0.65f } } },
        { "Future Rave Chest Thump (C#1)", {
            { params::kNote, 1.0f }, { params::kStartPitch, 510.0f }, { params::kPitchDecay, 39.0f },
            { params::kTailLength, 350.0f }, { params::kPunch, 0.75f }, { params::kPumpRate, 0.0f },
            { params::kPumpMix, 0.25f } } },
        { "Synced 1/16 Beat Stutter Kick (D1)", {
            { params::kNote, 2.0f }, { params::kTailLength, 310.0f }, { params::kStutterRate, 2.0f },
            { params::kStutterMix, 0.65f }, { params::kDrive, 0.3f } } },
        { "Trance Gate Chopper (A#1)", {
            { params::kNote, 10.0f }, { params::kTailLength, 520.0f }, { params::kGateRate, 2.0f },
            { params::kGateMix, 0.75f }, { params::kDrive, 0.25f } } },
        { "Deep Minimal Sub (B1)", {
            { params::kNote, 11.0f }, { params::kStartPitch, 260.0f }, { params::kPitchDecay, 28.0f },
            { params::kTailLength, 330.0f }, { params::kClickLevel, 0.15f }, { params::kPunch, 0.35f } } },
        makeMultiSlotBuildPreset("Festival 1/64 Drop Rush (8 Bar)", 5.0f, 380.0f, 3.0f, 2.0f, 0.75f, 6.0f, 0.65f, 0.3f),
        makeMultiSlotBuildPreset("Mainstage Build Up (16 Bar)", 7.0f, 400.0f, 4.0f, 1.0f, 0.72f, 8.0f, 0.75f, 0.32f),
        makeMultiSlotBuildPreset("Arena 32-Bar Riser (32 Bar)", 9.0f, 420.0f, 5.0f, 4.0f, 0.78f, 12.0f, 0.85f, 0.35f),
        makeMultiSlotBuildPreset("Marathon 64-Bar Odyssey (64 Bar)", 0.0f, 410.0f, 6.0f, 9.0f, 0.80f, 12.0f, 0.90f, 0.35f),
        makeMultiSlotBuildPreset("Quick 2-Bar Turnaround Fill (2 Bar)", 4.0f, 340.0f, 1.0f, 0.0f, 0.68f, 3.0f, 0.35f, 0.22f),
        makeMultiSlotBuildPreset("1-Bar Machine Gun Fill (1 Bar)", 2.0f, 360.0f, 0.0f, 11.0f, 0.85f, 5.0f, 0.55f, 0.30f),
        { "Stacked Snare Clap Hat (F1)", {
            { params::kNote, 5.0f }, { params::kTailLength, 420.0f }, { params::kPunch, 0.5f },
            { params::kSnareLevel, 0.58f }, { params::kSnareTone, 0.64f }, { params::kSnareDecay, 0.42f },
            { params::kClapLevel, 0.40f }, { params::kClapSpread, 0.52f }, { params::kClapTone, 0.68f },
            { params::kHatLevel, 0.34f }, { params::kHatTone, 0.74f }, { params::kHatDecay, 0.22f }
        } }
    };
    return presets;
}

const std::vector<FactoryPreset>& fillBuildPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> list;
        const std::array<float, core::kNumFillPresets> defaultShorten{
            0.55f, 0.68f, 0.82f, 0.50f, 0.76f, 0.70f, 0.58f, 0.72f, 0.45f, 0.80f, 0.78f, 0.88f
        };
        const std::array<float, core::kNumFillPresets> defaultPitchRise{
            0.0f, 4.0f, 7.0f, 0.0f, 6.0f, 12.0f, 2.0f, 5.0f, 0.0f, 8.0f, 5.0f, 9.0f
        };
        std::ranges::for_each(std::views::iota(0, core::kNumFillPresets), [&](int idx) {
            list.push_back(FactoryPreset{
                core::kFillPresetNames[static_cast<size_t>(idx)].data(),
                {
                    { params::kAutoEnabled, 1.0f },
                    { params::kAutoBypass, 0.0f },
                    { params::kAutoFillPreset, static_cast<float>(idx) },
                    { params::kAutoGrid, 4.0f },
                    { params::kAutoTailShorten, defaultShorten[static_cast<size_t>(idx)] },
                    { params::kAutoPitchRise, defaultPitchRise[static_cast<size_t>(idx)] }
                }
            });
        });
        return list;
    }();
    return presets;
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    std::ranges::for_each(state.processor.getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr) {
            return;
        }
        const auto match = std::ranges::find_if(preset.values,
            [&](const auto& value) { return ranged->paramID == value.first; });
        if (match == preset.values.end()) {
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
            return;
        }
        ranged->setValueNotifyingHost(ranged->convertTo0to1(match->second));
    });
}

void applyToCurrentAutoSlot(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    const int slot = std::clamp(
        static_cast<int>(state.getRawParameterValue(params::kAutoSlot)->load()),
        0,
        core::kNumAutoSlots - 1);
    std::ranges::for_each(preset.values, [&](const auto& entry) {
        const bool isPerSlot = std::ranges::any_of(params::kAutoSlotIds, [&](const char* id) {
            return juce::String(id) == entry.first;
        });
        const auto paramId = isPerSlot ? params::autoSlotId(slot, entry.first) : entry.first;
        if (auto* ranged = state.getParameter(paramId)) {
            ranged->setValueNotifyingHost(ranged->convertTo0to1(entry.second));
        }
    });
}

} // namespace openedmkick::presets
