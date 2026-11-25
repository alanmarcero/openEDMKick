#include "Presets.h"

#include "AutoSequencer.h"
#include "KickSynth.h"
#include "Parameters.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <ranges>
#include <iterator>

namespace openedmkick::presets {
namespace {

using core::ClickType;

struct KickProfile {
    const char* name;
    ClickType click;
    float sweepOct, sweepMs, sweepShape;
    float bodyTone, knockHz, knockDb, toneHz;
    float clickLevel, clickHz, clickMs, clickPunch;
    float holdMs, decayMs, decayShape, sub, subOctave, lowCutHz, width;
};

constexpr std::array<KickProfile, 56> kProfiles{{
    // Uplifting: punchy, airy, melodic trance kicks
    { "Uplifting / First Light",    ClickType::beater, 2.4f, 33,  0.16f, 0.49f, 142, 3.2f, 12500, 0.34f, 4600, 3.1f, 0.15f, 17, 155, -0.22f, 0.65f, 0.00f, 20, 0.05f },
    { "Uplifting / Glass Beater",   ClickType::tick,   2.8f, 26,  0.35f, 0.62f, 165, 5.0f, 15500, 0.48f, 7400, 2.2f, 0.42f, 14, 132, -0.35f, 0.56f, 0.00f, 24, 0.18f },
    { "Uplifting / Wide Open",      ClickType::beater, 2.1f, 44,  0.05f, 0.38f, 128, 3.5f, 11000, 0.30f, 3900, 3.8f, 0.10f, 28, 220, -0.15f, 0.74f, 0.22f, 20, 0.28f },
    { "Uplifting / Downbeat",       ClickType::analog, 2.5f, 31,  0.25f, 0.54f, 150, 4.5f, 12000, 0.38f, 5100, 3.0f, 0.32f, 20, 148, -0.26f, 0.62f, 0.00f, 22, 0.10f },
    { "Uplifting / Top Line",       ClickType::noise,  3.1f, 22,  0.45f, 0.72f, 185, 6.2f, 17000, 0.55f, 8200, 2.5f, 0.50f, 11, 118, -0.40f, 0.52f, 0.00f, 28, 0.24f },
    { "Uplifting / Clean Lift",     ClickType::beater, 1.9f, 38,  0.10f, 0.26f, 118, 2.0f,  9200, 0.25f, 4100, 3.4f, 0.05f, 22, 185, -0.18f, 0.72f, 0.12f, 20, 0.00f },
    { "Uplifting / Short Arc",      ClickType::tick,   2.6f, 21,  0.40f, 0.58f, 170, 5.5f, 13800, 0.44f, 5800, 1.8f, 0.38f, 10, 105, -0.45f, 0.58f, 0.00f, 26, 0.12f },
    { "Uplifting / Main Room",      ClickType::beater, 2.7f, 36,  0.20f, 0.66f, 148, 6.8f, 14500, 0.46f, 5200, 3.5f, 0.45f, 32, 240, -0.12f, 0.70f, 0.28f, 20, 0.32f },
    { "Uplifting / Euphoria",       ClickType::beater, 2.9f, 29,  0.28f, 0.60f, 156, 4.8f, 16000, 0.45f, 6200, 2.6f, 0.36f, 16, 142, -0.28f, 0.64f, 0.10f, 22, 0.22f },
    { "Uplifting / Morning Sun",    ClickType::analog, 2.2f, 34,  0.14f, 0.40f, 136, 3.0f, 10600, 0.30f, 3700, 3.3f, 0.12f, 20, 168, -0.20f, 0.72f, 0.16f, 20, 0.08f },
    { "Uplifting / Skyward",        ClickType::tick,   3.0f, 24,  0.42f, 0.68f, 175, 5.8f, 16800, 0.50f, 7800, 2.1f, 0.44f, 13, 128, -0.36f, 0.58f, 0.00f, 26, 0.26f },
    { "Uplifting / Pure 138",       ClickType::beater, 2.3f, 28,  0.22f, 0.46f, 144, 3.4f, 11800, 0.36f, 4900, 2.8f, 0.24f, 15, 138, -0.30f, 0.68f, 0.00f, 24, 0.06f },
    { "Uplifting / Silver Lining",  ClickType::tick,   2.7f, 20,  0.38f, 0.56f, 178, 5.2f, 14600, 0.46f, 6700, 1.9f, 0.40f, 11, 112, -0.42f, 0.60f, 0.00f, 26, 0.14f },
    { "Uplifting / Afterglow",      ClickType::analog, 2.0f, 40,  0.08f, 0.34f, 124, 3.8f,  9800, 0.26f, 3400, 4.0f, 0.08f, 26, 205, -0.16f, 0.78f, 0.24f, 20, 0.12f },
    { "Uplifting / Starlight",      ClickType::noise,  3.2f, 25,  0.36f, 0.64f, 168, 6.0f, 15800, 0.52f, 7100, 2.7f, 0.46f, 18, 150, -0.25f, 0.62f, 0.08f, 24, 0.30f },
    { "Uplifting / Horizon",        ClickType::beater, 2.4f, 35,  0.18f, 0.44f, 134, 3.6f, 13200, 0.35f, 4500, 3.2f, 0.18f, 24, 192, -0.19f, 0.70f, 0.18f, 20, 0.22f },

    // Rolling: tight, rhythmic, sub-locked bass-friendly kicks
    { "Rolling / Pocket",           ClickType::tick,   1.9f, 26,  0.20f, 0.35f, 118, 2.2f,  7600, 0.28f, 3800, 2.0f, 0.20f, 13, 120, -0.35f, 0.75f, 0.00f, 25, 0.00f },
    { "Rolling / Offbeat",          ClickType::beater, 2.2f, 29,  0.30f, 0.46f, 132, 4.2f,  9800, 0.36f, 4600, 2.4f, 0.35f, 16, 134, -0.30f, 0.68f, 0.00f, 28, 0.08f },
    { "Rolling / Undertow",         ClickType::analog, 1.5f, 34, -0.15f, 0.22f,  92, 4.0f,  4200, 0.18f, 2400, 3.2f, 0.05f, 26, 195, -0.22f, 0.88f, 0.36f, 20, 0.00f },
    { "Rolling / Needle",           ClickType::tick,   2.6f, 18,  0.55f, 0.54f, 160, 5.4f, 14800, 0.52f, 7200, 1.2f, 0.55f,  8,  98, -0.50f, 0.62f, 0.00f, 32, 0.15f },
    { "Rolling / Low Rider",        ClickType::analog, 1.6f, 32,  0.00f, 0.28f,  98, 3.8f,  5400, 0.22f, 2900, 3.6f, 0.12f, 22, 165, -0.28f, 0.84f, 0.26f, 20, 0.00f },
    { "Rolling / Slipstream",       ClickType::noise,  2.3f, 24,  0.32f, 0.48f, 138, 4.6f, 11200, 0.42f, 5100, 2.2f, 0.30f, 14, 126, -0.34f, 0.70f, 0.00f, 26, 0.14f },
    { "Rolling / Bass Stitch",      ClickType::tick,   1.7f, 22,  0.25f, 0.30f, 124, 2.8f,  6800, 0.32f, 4200, 1.6f, 0.28f, 10, 108, -0.42f, 0.78f, 0.00f, 34, 0.00f },
    { "Rolling / Late Shift",       ClickType::beater, 2.0f, 35,  0.10f, 0.52f, 116, 5.2f,  8200, 0.30f, 3600, 3.4f, 0.22f, 24, 175, -0.20f, 0.76f, 0.16f, 22, 0.12f },

    // Progressive: warm, deep, rounded, musical
    { "Progressive / Soft Edge",    ClickType::analog, 1.4f, 40, -0.10f, 0.18f,  96, 2.2f,  4600, 0.16f, 2200, 4.2f, 0.00f, 24, 190, -0.24f, 0.80f, 0.14f, 20, 0.00f },
    { "Progressive / Indigo",       ClickType::beater, 1.8f, 45,  0.00f, 0.30f, 108, 3.0f,  5800, 0.22f, 2800, 4.0f, 0.08f, 30, 235, -0.16f, 0.76f, 0.20f, 20, 0.10f },
    { "Progressive / Closed Beater",ClickType::analog, 2.0f, 28,  0.18f, 0.36f, 122, 3.4f,  3600, 0.26f, 2100, 3.2f, 0.18f, 16, 145, -0.30f, 0.72f, 0.00f, 22, 0.00f },
    { "Progressive / Undercurrent", ClickType::analog, 1.3f, 38, -0.25f, 0.15f,  86, 4.8f,  3200, 0.12f, 1800, 5.0f, 0.00f, 36, 260, -0.14f, 0.90f, 0.38f, 20, 0.00f },
    { "Progressive / Blue Hour",    ClickType::beater, 2.1f, 37,  0.12f, 0.42f, 114, 3.6f,  7400, 0.28f, 3600, 3.6f, 0.16f, 22, 180, -0.22f, 0.74f, 0.12f, 20, 0.16f },
    { "Progressive / Velvet Sub",   ClickType::analog, 1.6f, 34, -0.18f, 0.12f,  90, 2.6f,  2600, 0.14f, 1600, 4.5f, 0.00f, 34, 245, -0.18f, 0.88f, 0.32f, 20, 0.00f },
    { "Progressive / Slow Rise",    ClickType::beater, 1.9f, 52, -0.05f, 0.34f, 112, 4.2f,  6200, 0.24f, 3100, 4.4f, 0.10f, 38, 280, -0.10f, 0.78f, 0.22f, 20, 0.18f },
    { "Progressive / Muted Tap",    ClickType::tick,   1.5f, 25,  0.15f, 0.24f, 128, 2.4f,  4100, 0.28f, 3200, 2.5f, 0.20f, 14, 128, -0.34f, 0.74f, 0.00f, 24, 0.00f },

    // Psy: laser zap, fast pitch dive, crisp attack
    { "Psy / Triplet",              ClickType::tick,   3.4f, 18,  0.55f, 0.44f, 152, 4.8f, 14200, 0.46f, 6400, 1.4f, 0.48f, 10, 102, -0.45f, 0.66f, 0.00f, 28, 0.10f },
    { "Psy / Pinpoint",             ClickType::tick,   4.2f, 14,  0.68f, 0.56f, 180, 6.0f, 16500, 0.58f, 8800, 1.0f, 0.62f,  8,  95, -0.55f, 0.60f, 0.00f, 32, 0.16f },
    { "Psy / Elastic",              ClickType::beater, 2.9f, 30,  0.38f, 0.50f, 138, 5.4f, 11800, 0.38f, 4800, 2.2f, 0.36f, 15, 136, -0.25f, 0.72f, 0.00f, 24, 0.12f },
    { "Psy / Short Circuit",        ClickType::noise,  3.8f, 19,  0.60f, 0.68f, 195, 7.2f, 15600, 0.50f, 7500, 1.5f, 0.54f, 11, 112, -0.42f, 0.64f, 0.00f, 30, 0.22f },
    { "Psy / Ground Wire",          ClickType::tick,   2.6f, 24,  0.42f, 0.38f, 126, 4.4f, 10400, 0.40f, 5200, 1.8f, 0.38f, 14, 125, -0.32f, 0.78f, 0.14f, 24, 0.06f },
    { "Psy / Acid Tick",            ClickType::tick,   4.5f, 16,  0.70f, 0.74f, 210, 7.8f, 17500, 0.60f, 9400, 1.3f, 0.60f, 12, 118, -0.38f, 0.62f, 0.00f, 28, 0.20f },
    { "Psy / Full On",              ClickType::beater, 3.2f, 25,  0.48f, 0.58f, 162, 6.2f, 13600, 0.48f, 6600, 1.9f, 0.45f, 16, 138, -0.28f, 0.70f, 0.00f, 26, 0.18f },
    { "Psy / Microdose",            ClickType::tick,   2.8f, 16,  0.50f, 0.42f, 148, 3.8f, 12000, 0.44f, 6100, 1.1f, 0.42f,  7,  96, -0.52f, 0.68f, 0.00f, 30, 0.08f },

    // Tech: industrial knock & percussive layers
    { "Tech / Rivet",               ClickType::noise,  2.6f, 25,  0.34f, 0.68f, 182, 7.5f, 13200, 0.46f, 5800, 2.8f, 0.45f, 18, 145, -0.24f, 0.60f, 0.00f, 26, 0.18f },
    { "Tech / Dry Contact",         ClickType::tick,   2.3f, 22,  0.28f, 0.56f, 168, 5.6f, 10800, 0.42f, 4900, 2.1f, 0.38f, 12, 115, -0.38f, 0.65f, 0.00f, 30, 0.05f },
    { "Tech / Front Row",           ClickType::noise,  2.9f, 30,  0.30f, 0.76f, 174, 8.2f, 14800, 0.50f, 6800, 3.2f, 0.52f, 24, 175, -0.18f, 0.62f, 0.12f, 24, 0.28f },
    { "Tech / Machined",            ClickType::beater, 2.7f, 24,  0.40f, 0.72f, 205, 8.8f, 15200, 0.48f, 7100, 2.5f, 0.50f, 15, 130, -0.30f, 0.58f, 0.00f, 28, 0.16f },
    { "Tech / Dark Room",           ClickType::analog, 1.9f, 35,  0.08f, 0.52f, 118, 6.4f,  3800, 0.26f, 2600, 3.8f, 0.18f, 28, 210, -0.15f, 0.78f, 0.26f, 20, 0.14f },
    { "Tech / Knuckle",             ClickType::noise,  2.5f, 27,  0.32f, 0.64f, 160, 6.8f,  9600, 0.44f, 4400, 2.9f, 0.40f, 16, 140, -0.26f, 0.66f, 0.00f, 26, 0.15f },
    { "Tech / Staccato",            ClickType::tick,   3.1f, 19,  0.52f, 0.70f, 220, 7.6f, 14400, 0.54f, 6900, 1.6f, 0.58f, 10, 102, -0.48f, 0.58f, 0.00f, 34, 0.12f },
    { "Tech / Pressure Plate",      ClickType::noise,  2.2f, 32,  0.22f, 0.62f, 146, 9.2f, 11400, 0.40f, 5100, 3.0f, 0.35f, 22, 168, -0.20f, 0.70f, 0.18f, 22, 0.24f },

    // Classic: 909 / 808 / analog hardware vibe
    { "Classic / Vinyl Tap",        ClickType::analog, 2.0f, 37,  0.12f, 0.42f, 148, 3.6f,  6800, 0.28f, 3300, 4.2f, 0.12f, 18, 160, -0.28f, 0.70f, 0.00f, 22, 0.05f },
    { "Classic / Nine O Nine",      ClickType::analog, 2.4f, 41,  0.22f, 0.58f, 164, 5.8f,  9400, 0.38f, 4100, 4.4f, 0.34f, 22, 178, -0.24f, 0.64f, 0.00f, 24, 0.14f },
    { "Classic / White Label",      ClickType::beater, 2.1f, 35,  0.18f, 0.46f, 152, 4.2f,  8600, 0.34f, 4300, 3.6f, 0.22f, 17, 150, -0.30f, 0.68f, 0.00f, 22, 0.08f },
    { "Classic / Gatefold",         ClickType::analog, 1.7f, 33,  0.04f, 0.34f, 134, 3.8f,  5200, 0.22f, 2900, 4.0f, 0.08f, 20, 145, -0.28f, 0.76f, 0.16f, 20, 0.00f },
    { "Classic / Warehouse",        ClickType::beater, 2.5f, 39,  0.24f, 0.62f, 158, 6.4f, 10800, 0.40f, 4700, 3.8f, 0.32f, 26, 195, -0.18f, 0.68f, 0.20f, 22, 0.26f },
    { "Classic / Afterhours",       ClickType::analog, 1.8f, 36,  0.06f, 0.32f, 116, 4.4f,  4400, 0.20f, 2700, 4.6f, 0.05f, 28, 215, -0.16f, 0.80f, 0.24f, 20, 0.10f },
    { "Classic / Dub Cut",          ClickType::analog, 1.5f, 30, -0.10f, 0.24f,  98, 3.2f,  2800, 0.16f, 2200, 3.8f, 0.00f, 22, 165, -0.24f, 0.84f, 0.30f, 20, 0.00f },
    { "Classic / Final Press",      ClickType::beater, 2.2f, 34,  0.20f, 0.50f, 150, 5.0f,  9200, 0.36f, 4050, 3.5f, 0.28f, 18, 152, -0.26f, 0.70f, 0.10f, 24, 0.12f }
}};

[[nodiscard]] float outputTrimDb(const char* name) {
    // These hits were reaching the limiter. The trim keeps that ceiling out of the sound.
    if (std::string_view(name) == "Uplifting / Skyward") return -2.4f;
    if (std::string_view(name) == "Progressive / Undercurrent") return -4.0f;
    if (std::string_view(name) == "Psy / Short Circuit") return -3.6f;
    if (std::string_view(name) == "Psy / Acid Tick") return -4.0f;
    if (std::string_view(name) == "Tech / Machined") return -3.6f;
    if (std::string_view(name) == "Tech / Pressure Plate") return -4.0f;
    return 0.0f;
}

[[nodiscard]] FactoryPreset makeFactoryPreset(const KickProfile& kick) {
    std::vector<std::pair<juce::String, float>> values{
        { params::kSweepOctaves, kick.sweepOct }, { params::kSweepMs, kick.sweepMs },
        { params::kSweepShape, kick.sweepShape }, { params::kSnapOctaves, kick.sweepOct * 0.47f },
        { params::kSnapMs, kick.clickMs * 0.6f }, { params::kBodyTone, kick.bodyTone },
        { params::kKnockHz, kick.knockHz }, { params::kKnockDb, kick.knockDb },
        { params::kToneHz, kick.toneHz },
        { params::kSubLevel, kick.sub }, { params::kStartPhase, 0.18f },
        { params::kClickType, static_cast<float>(kick.click) }, { params::kClickLevel, kick.clickLevel },
        { params::kClickHz, kick.clickHz }, { params::kClickMs, kick.clickMs },
        { params::kAttackMs, 0.24f }, { params::kHoldMs, kick.holdMs },
        { params::kDecayMs, kick.decayMs }, { params::kDecayShape, kick.decayShape },
        { params::kLowCutHz, kick.lowCutHz },
        { params::kAmpEnabled, 0 }, { params::kAmpAttack, 0 }, { params::kAmpDecay, 100 },
        { params::kAmpSustain, 0.8f }, { params::kAmpRelease, 300 },
        { params::kSubOctave, kick.subOctave }, { params::kClickPunch, kick.clickPunch },
        { params::kStereoWidth, kick.width },
        { params::kOutput, outputTrimDb(kick.name) },
        { params::kEqLowFreq, params::eqNoteToHz(40) }, { params::kEqLowDb, 0.0f },
        { params::kEqMidFreq, params::eqNoteToHz(83) }, { params::kEqMidDb, 0.0f },
        { params::kEqHighFreq, params::eqNoteToHz(118) }, { params::kEqHighDb, 0.0f },
        { params::kHighPass, 20.0f }, { params::kLowPass, 20000.0f }
    };
    return { kick.name, std::move(values) };
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
    static const std::vector<FactoryPreset> presets;
    return presets;
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    std::ranges::for_each(preset.values, [&](const auto& value) {
        if (auto* parameter = state.getParameter(value.first))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value.second));
    });
}

} // namespace openedmkick::presets
