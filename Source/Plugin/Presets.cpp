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
using core::DrumSoundType;
using core::Saturation;

struct LayerPreset {
    bool enabled = false;
    DrumSoundType sound = DrumSoundType::hat;
    float gain = 0.45f;
    float transpose = 0.0f;
    float decayMs = 80.0f;
    float hpHz = 20.0f;
    float lpHz = 20000.0f;
};

struct KickProfile {
    const char* name;
    Saturation sat;
    ClickType click;
    float sweepOct, sweepMs, sweepShape;
    float bodyTone, drive, grit, knockHz, knockDb, toneHz, clip;
    float clickLevel, clickHz, clickMs, clickPunch;
    float holdMs, decayMs, decayShape, sub, subOctave, lowCutHz, width;
    LayerPreset layerB{};
    LayerPreset layerC{};
};

constexpr LayerPreset none{};
constexpr LayerPreset crispHat{ true, DrumSoundType::hat, 0.38f, 4.0f, 38.0f, 5500.0f, 18000.0f };
constexpr LayerPreset openHat{ true, DrumSoundType::hat, 0.42f, 0.0f, 75.0f, 4200.0f, 16000.0f };
constexpr LayerPreset tightClap{ true, DrumSoundType::clap, 0.45f, 0.0f, 42.0f, 950.0f, 11000.0f };
constexpr LayerPreset wideClap{ true, DrumSoundType::clap, 0.50f, 3.0f, 68.0f, 800.0f, 14000.0f };
constexpr LayerPreset punchSnare{ true, DrumSoundType::snare, 0.44f, 2.0f, 48.0f, 220.0f, 9500.0f };
constexpr LayerPreset snapSnare{ true, DrumSoundType::snare, 0.52f, 5.0f, 32.0f, 450.0f, 12000.0f };
constexpr LayerPreset subLayer{ true, DrumSoundType::kick, 0.45f, -12.0f, 240.0f, 20.0f, 160.0f };

constexpr std::array<KickProfile, 56> kProfiles{{
    // Uplifting: punchy, airy, melodic trance kicks
    { "Uplifting / First Light",    Saturation::warm,   ClickType::beater, 2.4f, 33,  0.16f, 0.49f, 0.14f, 0.06f, 142, 3.2f, 12500, 0.06f, 0.34f, 4600, 3.1f, 0.15f, 17, 155, -0.22f, 0.65f, 0.00f, 20, 0.05f, none, none },
    { "Uplifting / Glass Beater",   Saturation::tube,   ClickType::tick,   2.8f, 26,  0.35f, 0.62f, 0.28f, 0.12f, 165, 5.0f, 15500, 0.18f, 0.48f, 7400, 2.2f, 0.42f, 14, 132, -0.35f, 0.56f, 0.00f, 24, 0.18f, crispHat, none },
    { "Uplifting / Wide Open",      Saturation::warm,   ClickType::beater, 2.1f, 44,  0.05f, 0.38f, 0.18f, 0.10f, 128, 3.5f, 11000, 0.10f, 0.30f, 3900, 3.8f, 0.10f, 28, 220, -0.15f, 0.74f, 0.22f, 20, 0.28f, openHat, none },
    { "Uplifting / Downbeat",       Saturation::tube,   ClickType::analog, 2.5f, 31,  0.25f, 0.54f, 0.32f, 0.15f, 150, 4.5f, 12000, 0.20f, 0.38f, 5100, 3.0f, 0.32f, 20, 148, -0.26f, 0.62f, 0.00f, 22, 0.10f, none, none },
    { "Uplifting / Top Line",       Saturation::warm,   ClickType::noise,  3.1f, 22,  0.45f, 0.72f, 0.24f, 0.08f, 185, 6.2f, 17000, 0.14f, 0.55f, 8200, 2.5f, 0.50f, 11, 118, -0.40f, 0.52f, 0.00f, 28, 0.24f, crispHat, tightClap },
    { "Uplifting / Clean Lift",     Saturation::warm,   ClickType::beater, 1.9f, 38,  0.10f, 0.26f, 0.08f, 0.03f, 118, 2.0f,  9200, 0.03f, 0.25f, 4100, 3.4f, 0.05f, 22, 185, -0.18f, 0.72f, 0.12f, 20, 0.00f, none, none },
    { "Uplifting / Short Arc",      Saturation::tube,   ClickType::tick,   2.6f, 21,  0.40f, 0.58f, 0.36f, 0.18f, 170, 5.5f, 13800, 0.22f, 0.44f, 5800, 1.8f, 0.38f, 10, 105, -0.45f, 0.58f, 0.00f, 26, 0.12f, none, none },
    { "Uplifting / Main Room",      Saturation::tube,   ClickType::beater, 2.7f, 36,  0.20f, 0.66f, 0.44f, 0.24f, 148, 6.8f, 14500, 0.32f, 0.46f, 5200, 3.5f, 0.45f, 32, 240, -0.12f, 0.70f, 0.28f, 20, 0.32f, wideClap, crispHat },
    { "Uplifting / Euphoria",       Saturation::tube,   ClickType::beater, 2.9f, 29,  0.28f, 0.60f, 0.30f, 0.14f, 156, 4.8f, 16000, 0.16f, 0.45f, 6200, 2.6f, 0.36f, 16, 142, -0.28f, 0.64f, 0.10f, 22, 0.22f, crispHat, none },
    { "Uplifting / Morning Sun",    Saturation::warm,   ClickType::analog, 2.2f, 34,  0.14f, 0.40f, 0.16f, 0.08f, 136, 3.0f, 10600, 0.08f, 0.30f, 3700, 3.3f, 0.12f, 20, 168, -0.20f, 0.72f, 0.16f, 20, 0.08f, none, none },
    { "Uplifting / Skyward",        Saturation::tube,   ClickType::tick,   3.0f, 24,  0.42f, 0.68f, 0.34f, 0.16f, 175, 5.8f, 16800, 0.20f, 0.50f, 7800, 2.1f, 0.44f, 13, 128, -0.36f, 0.58f, 0.00f, 26, 0.26f, openHat, tightClap },
    { "Uplifting / Pure 138",       Saturation::warm,   ClickType::beater, 2.3f, 28,  0.22f, 0.46f, 0.18f, 0.07f, 144, 3.4f, 11800, 0.09f, 0.36f, 4900, 2.8f, 0.24f, 15, 138, -0.30f, 0.68f, 0.00f, 24, 0.06f, none, none },
    { "Uplifting / Silver Lining",  Saturation::tube,   ClickType::tick,   2.7f, 20,  0.38f, 0.56f, 0.26f, 0.11f, 178, 5.2f, 14600, 0.15f, 0.46f, 6700, 1.9f, 0.40f, 11, 112, -0.42f, 0.60f, 0.00f, 26, 0.14f, crispHat, none },
    { "Uplifting / Afterglow",      Saturation::warm,   ClickType::analog, 2.0f, 40,  0.08f, 0.34f, 0.20f, 0.11f, 124, 3.8f,  9800, 0.12f, 0.26f, 3400, 4.0f, 0.08f, 26, 205, -0.16f, 0.78f, 0.24f, 20, 0.12f, snapSnare, none },
    { "Uplifting / Starlight",      Saturation::tube,   ClickType::noise,  3.2f, 25,  0.36f, 0.64f, 0.38f, 0.19f, 168, 6.0f, 15800, 0.24f, 0.52f, 7100, 2.7f, 0.46f, 18, 150, -0.25f, 0.62f, 0.08f, 24, 0.30f, wideClap, none },
    { "Uplifting / Horizon",        Saturation::warm,   ClickType::beater, 2.4f, 35,  0.18f, 0.44f, 0.22f, 0.12f, 134, 3.6f, 13200, 0.13f, 0.35f, 4500, 3.2f, 0.18f, 24, 192, -0.19f, 0.70f, 0.18f, 20, 0.22f, openHat, none },

    // Rolling: tight, rhythmic, sub-locked bass-friendly kicks
    { "Rolling / Pocket",           Saturation::warm,   ClickType::tick,   1.9f, 26,  0.20f, 0.35f, 0.12f, 0.05f, 118, 2.2f,  7600, 0.06f, 0.28f, 3800, 2.0f, 0.20f, 13, 120, -0.35f, 0.75f, 0.00f, 25, 0.00f, none, none },
    { "Rolling / Offbeat",          Saturation::tube,   ClickType::beater, 2.2f, 29,  0.30f, 0.46f, 0.28f, 0.14f, 132, 4.2f,  9800, 0.16f, 0.36f, 4600, 2.4f, 0.35f, 16, 134, -0.30f, 0.68f, 0.00f, 28, 0.08f, crispHat, none },
    { "Rolling / Undertow",         Saturation::warm,   ClickType::analog, 1.5f, 34, -0.15f, 0.22f, 0.16f, 0.08f,  92, 4.0f,  4200, 0.08f, 0.18f, 2400, 3.2f, 0.05f, 26, 195, -0.22f, 0.88f, 0.36f, 20, 0.00f, subLayer, none },
    { "Rolling / Needle",           Saturation::crunch, ClickType::tick,   2.6f, 18,  0.55f, 0.54f, 0.38f, 0.15f, 160, 5.4f, 14800, 0.24f, 0.52f, 7200, 1.2f, 0.55f,  8,  98, -0.50f, 0.62f, 0.00f, 32, 0.15f, none, none },
    { "Rolling / Low Rider",        Saturation::warm,   ClickType::analog, 1.6f, 32,  0.00f, 0.28f, 0.22f, 0.12f,  98, 3.8f,  5400, 0.12f, 0.22f, 2900, 3.6f, 0.12f, 22, 165, -0.28f, 0.84f, 0.26f, 20, 0.00f, none, none },
    { "Rolling / Slipstream",       Saturation::tube,   ClickType::noise,  2.3f, 24,  0.32f, 0.48f, 0.30f, 0.16f, 138, 4.6f, 11200, 0.18f, 0.42f, 5100, 2.2f, 0.30f, 14, 126, -0.34f, 0.70f, 0.00f, 26, 0.14f, tightClap, none },
    { "Rolling / Bass Stitch",      Saturation::warm,   ClickType::tick,   1.7f, 22,  0.25f, 0.30f, 0.18f, 0.06f, 124, 2.8f,  6800, 0.10f, 0.32f, 4200, 1.6f, 0.28f, 10, 108, -0.42f, 0.78f, 0.00f, 34, 0.00f, none, none },
    { "Rolling / Late Shift",       Saturation::tube,   ClickType::beater, 2.0f, 35,  0.10f, 0.52f, 0.40f, 0.28f, 116, 5.2f,  8200, 0.25f, 0.30f, 3600, 3.4f, 0.22f, 24, 175, -0.20f, 0.76f, 0.16f, 22, 0.12f, punchSnare, none },

    // Progressive: warm, deep, rounded, musical
    { "Progressive / Soft Edge",    Saturation::warm,   ClickType::analog, 1.4f, 40, -0.10f, 0.18f, 0.08f, 0.04f,  96, 2.2f,  4600, 0.04f, 0.16f, 2200, 4.2f, 0.00f, 24, 190, -0.24f, 0.80f, 0.14f, 20, 0.00f, none, none },
    { "Progressive / Indigo",       Saturation::warm,   ClickType::beater, 1.8f, 45,  0.00f, 0.30f, 0.16f, 0.10f, 108, 3.0f,  5800, 0.08f, 0.22f, 2800, 4.0f, 0.08f, 30, 235, -0.16f, 0.76f, 0.20f, 20, 0.10f, none, none },
    { "Progressive / Closed Beater",Saturation::tube,   ClickType::analog, 2.0f, 28,  0.18f, 0.36f, 0.24f, 0.08f, 122, 3.4f,  3600, 0.12f, 0.26f, 2100, 3.2f, 0.18f, 16, 145, -0.30f, 0.72f, 0.00f, 22, 0.00f, none, none },
    { "Progressive / Undercurrent", Saturation::warm,   ClickType::analog, 1.3f, 38, -0.25f, 0.15f, 0.12f, 0.08f,  86, 4.8f,  3200, 0.05f, 0.12f, 1800, 5.0f, 0.00f, 36, 260, -0.14f, 0.90f, 0.38f, 20, 0.00f, subLayer, none },
    { "Progressive / Blue Hour",    Saturation::tube,   ClickType::beater, 2.1f, 37,  0.12f, 0.42f, 0.26f, 0.14f, 114, 3.6f,  7400, 0.14f, 0.28f, 3600, 3.6f, 0.16f, 22, 180, -0.22f, 0.74f, 0.12f, 20, 0.16f, openHat, none },
    { "Progressive / Velvet Sub",   Saturation::warm,   ClickType::analog, 1.6f, 34, -0.18f, 0.12f, 0.10f, 0.06f,  90, 2.6f,  2600, 0.04f, 0.14f, 1600, 4.5f, 0.00f, 34, 245, -0.18f, 0.88f, 0.32f, 20, 0.00f, none, none },
    { "Progressive / Slow Rise",    Saturation::tube,   ClickType::beater, 1.9f, 52, -0.05f, 0.34f, 0.28f, 0.20f, 112, 4.2f,  6200, 0.16f, 0.24f, 3100, 4.4f, 0.10f, 38, 280, -0.10f, 0.78f, 0.22f, 20, 0.18f, wideClap, none },
    { "Progressive / Muted Tap",    Saturation::warm,   ClickType::tick,   1.5f, 25,  0.15f, 0.24f, 0.14f, 0.05f, 128, 2.4f,  4100, 0.08f, 0.28f, 3200, 2.5f, 0.20f, 14, 128, -0.34f, 0.74f, 0.00f, 24, 0.00f, none, none },

    // Psy: laser zap, fast pitch dive, crisp/fold/crunch bite
    { "Psy / Triplet",              Saturation::crunch, ClickType::tick,   3.4f, 18,  0.55f, 0.44f, 0.36f, 0.16f, 152, 4.8f, 14200, 0.24f, 0.46f, 6400, 1.4f, 0.48f, 10, 102, -0.45f, 0.66f, 0.00f, 28, 0.10f, crispHat, none },
    { "Psy / Pinpoint",             Saturation::fold,   ClickType::tick,   4.2f, 14,  0.68f, 0.56f, 0.42f, 0.18f, 180, 6.0f, 16500, 0.28f, 0.58f, 8800, 1.0f, 0.62f,  8,  95, -0.55f, 0.60f, 0.00f, 32, 0.16f, none, none },
    { "Psy / Elastic",              Saturation::tube,   ClickType::beater, 2.9f, 30,  0.38f, 0.50f, 0.46f, 0.26f, 138, 5.4f, 11800, 0.30f, 0.38f, 4800, 2.2f, 0.36f, 15, 136, -0.25f, 0.72f, 0.00f, 24, 0.12f, tightClap, none },
    { "Psy / Short Circuit",        Saturation::fold,   ClickType::noise,  3.8f, 19,  0.60f, 0.68f, 0.54f, 0.32f, 195, 7.2f, 15600, 0.36f, 0.50f, 7500, 1.5f, 0.54f, 11, 112, -0.42f, 0.64f, 0.00f, 30, 0.22f, snapSnare, none },
    { "Psy / Ground Wire",          Saturation::crunch, ClickType::tick,   2.6f, 24,  0.42f, 0.38f, 0.32f, 0.18f, 126, 4.4f, 10400, 0.20f, 0.40f, 5200, 1.8f, 0.38f, 14, 125, -0.32f, 0.78f, 0.14f, 24, 0.06f, none, none },
    { "Psy / Acid Tick",            Saturation::fold,   ClickType::tick,   4.5f, 16,  0.70f, 0.74f, 0.58f, 0.38f, 210, 7.8f, 17500, 0.40f, 0.60f, 9400, 1.3f, 0.60f, 12, 118, -0.38f, 0.62f, 0.00f, 28, 0.20f, crispHat, snapSnare },
    { "Psy / Full On",              Saturation::crunch, ClickType::beater, 3.2f, 25,  0.48f, 0.58f, 0.48f, 0.28f, 162, 6.2f, 13600, 0.32f, 0.48f, 6600, 1.9f, 0.45f, 16, 138, -0.28f, 0.70f, 0.00f, 26, 0.18f, none, none },
    { "Psy / Microdose",            Saturation::tube,   ClickType::tick,   2.8f, 16,  0.50f, 0.42f, 0.28f, 0.10f, 148, 3.8f, 12000, 0.16f, 0.44f, 6100, 1.1f, 0.42f,  7,  96, -0.52f, 0.68f, 0.00f, 30, 0.08f, none, none },

    // Tech: aggressive, distorted, industrial knock & percussive layers
    { "Tech / Rivet",               Saturation::crunch, ClickType::noise,  2.6f, 25,  0.34f, 0.68f, 0.58f, 0.36f, 182, 7.5f, 13200, 0.38f, 0.46f, 5800, 2.8f, 0.45f, 18, 145, -0.24f, 0.60f, 0.00f, 26, 0.18f, tightClap, crispHat },
    { "Tech / Dry Contact",         Saturation::tube,   ClickType::tick,   2.3f, 22,  0.28f, 0.56f, 0.42f, 0.20f, 168, 5.6f, 10800, 0.26f, 0.42f, 4900, 2.1f, 0.38f, 12, 115, -0.38f, 0.65f, 0.00f, 30, 0.05f, none, none },
    { "Tech / Front Row",           Saturation::tube,   ClickType::noise,  2.9f, 30,  0.30f, 0.76f, 0.66f, 0.44f, 174, 8.2f, 14800, 0.45f, 0.50f, 6800, 3.2f, 0.52f, 24, 175, -0.18f, 0.62f, 0.12f, 24, 0.28f, wideClap, none },
    { "Tech / Machined",            Saturation::crunch, ClickType::beater, 2.7f, 24,  0.40f, 0.72f, 0.62f, 0.38f, 205, 8.8f, 15200, 0.42f, 0.48f, 7100, 2.5f, 0.50f, 15, 130, -0.30f, 0.58f, 0.00f, 28, 0.16f, snapSnare, none },
    { "Tech / Dark Room",           Saturation::tube,   ClickType::analog, 1.9f, 35,  0.08f, 0.52f, 0.54f, 0.42f, 118, 6.4f,  3800, 0.34f, 0.26f, 2600, 3.8f, 0.18f, 28, 210, -0.15f, 0.78f, 0.26f, 20, 0.14f, subLayer, none },
    { "Tech / Knuckle",             Saturation::crunch, ClickType::noise,  2.5f, 27,  0.32f, 0.64f, 0.50f, 0.30f, 160, 6.8f,  9600, 0.32f, 0.44f, 4400, 2.9f, 0.40f, 16, 140, -0.26f, 0.66f, 0.00f, 26, 0.15f, punchSnare, tightClap },
    { "Tech / Staccato",            Saturation::fold,   ClickType::tick,   3.1f, 19,  0.52f, 0.70f, 0.48f, 0.22f, 220, 7.6f, 14400, 0.36f, 0.54f, 6900, 1.6f, 0.58f, 10, 102, -0.48f, 0.58f, 0.00f, 34, 0.12f, none, none },
    { "Tech / Pressure Plate",      Saturation::crunch, ClickType::noise,  2.2f, 32,  0.22f, 0.62f, 0.72f, 0.55f, 146, 9.2f, 11400, 0.52f, 0.40f, 5100, 3.0f, 0.35f, 22, 168, -0.20f, 0.70f, 0.18f, 22, 0.24f, openHat, none },

    // Classic: 909 / 808 / analog hardware vibe
    { "Classic / Vinyl Tap",        Saturation::warm,   ClickType::analog, 2.0f, 37,  0.12f, 0.42f, 0.20f, 0.10f, 148, 3.6f,  6800, 0.12f, 0.28f, 3300, 4.2f, 0.12f, 18, 160, -0.28f, 0.70f, 0.00f, 22, 0.05f, none, none },
    { "Classic / Nine O Nine",      Saturation::tube,   ClickType::analog, 2.4f, 41,  0.22f, 0.58f, 0.38f, 0.22f, 164, 5.8f,  9400, 0.28f, 0.38f, 4100, 4.4f, 0.34f, 22, 178, -0.24f, 0.64f, 0.00f, 24, 0.14f, tightClap, crispHat },
    { "Classic / White Label",      Saturation::warm,   ClickType::beater, 2.1f, 35,  0.18f, 0.46f, 0.26f, 0.14f, 152, 4.2f,  8600, 0.18f, 0.34f, 4300, 3.6f, 0.22f, 17, 150, -0.30f, 0.68f, 0.00f, 22, 0.08f, none, none },
    { "Classic / Gatefold",         Saturation::warm,   ClickType::analog, 1.7f, 33,  0.04f, 0.34f, 0.22f, 0.12f, 134, 3.8f,  5200, 0.14f, 0.22f, 2900, 4.0f, 0.08f, 20, 145, -0.28f, 0.76f, 0.16f, 20, 0.00f, none, none },
    { "Classic / Warehouse",        Saturation::tube,   ClickType::beater, 2.5f, 39,  0.24f, 0.62f, 0.48f, 0.32f, 158, 6.4f, 10800, 0.34f, 0.40f, 4700, 3.8f, 0.32f, 26, 195, -0.18f, 0.68f, 0.20f, 22, 0.26f, wideClap, punchSnare },
    { "Classic / Afterhours",       Saturation::warm,   ClickType::analog, 1.8f, 36,  0.06f, 0.32f, 0.28f, 0.18f, 116, 4.4f,  4400, 0.16f, 0.20f, 2700, 4.6f, 0.05f, 28, 215, -0.16f, 0.80f, 0.24f, 20, 0.10f, openHat, none },
    { "Classic / Dub Cut",          Saturation::warm,   ClickType::analog, 1.5f, 30, -0.10f, 0.24f, 0.18f, 0.10f,  98, 3.2f,  2800, 0.10f, 0.16f, 2200, 3.8f, 0.00f, 22, 165, -0.24f, 0.84f, 0.30f, 20, 0.00f, subLayer, none },
    { "Classic / Final Press",      Saturation::tube,   ClickType::beater, 2.2f, 34,  0.20f, 0.50f, 0.34f, 0.20f, 150, 5.0f,  9200, 0.24f, 0.36f, 4050, 3.5f, 0.28f, 18, 152, -0.26f, 0.70f, 0.10f, 24, 0.12f, crispHat, none }
}};

void appendLayer(std::vector<std::pair<juce::String, float>>& values, int layerIndex, const LayerPreset& layer) {
    const auto id = [layerIndex](const char* key) { return params::layerId(layerIndex, key); };
    values.emplace_back(id(params::kLayerEnabled), layer.enabled ? 1.0f : 0.0f);
    values.emplace_back(id(params::kLayerSound), static_cast<float>(layer.sound));
    values.emplace_back(id(params::kLayerGain), layer.gain);
    values.emplace_back(id(params::kLayerTranspose), layer.transpose);
    values.emplace_back(id(params::kLayerDecay), layer.decayMs);
    values.emplace_back(id(params::kLayerHighPass), layer.hpHz);
    values.emplace_back(id(params::kLayerLowPass), layer.lpHz);
    values.emplace_back(id(params::kLayerSolo), 0.0f);
    values.emplace_back(id(params::kLayerPan), 0.0f);
    values.emplace_back(id(params::kLayerPolarity), 0.0f);
}

[[nodiscard]] FactoryPreset makeFactoryPreset(const KickProfile& kick) {
    std::vector<std::pair<juce::String, float>> values{
        { params::kSweepOctaves, kick.sweepOct }, { params::kSweepMs, kick.sweepMs },
        { params::kSweepShape, kick.sweepShape }, { params::kSnapOctaves, kick.sweepOct * 0.47f },
        { params::kSnapMs, kick.clickMs * 0.6f }, { params::kBodyTone, kick.bodyTone },
        { params::kBodyDrive, kick.drive }, { params::kSaturation, static_cast<float>(kick.sat) },
        { params::kKnockHz, kick.knockHz }, { params::kKnockDb, kick.knockDb },
        { params::kGrit, kick.grit }, { params::kToneHz, kick.toneHz },
        { params::kSubLevel, kick.sub }, { params::kStartPhase, 0.18f },
        { params::kClickType, static_cast<float>(kick.click) }, { params::kClickLevel, kick.clickLevel },
        { params::kClickHz, kick.clickHz }, { params::kClickMs, kick.clickMs },
        { params::kAttackMs, 0.24f }, { params::kHoldMs, kick.holdMs },
        { params::kDecayMs, kick.decayMs }, { params::kDecayShape, kick.decayShape },
        { params::kLowCutHz, kick.lowCutHz }, { params::kClip, kick.clip },
        { params::kAmpEnabled, 0 }, { params::kAmpAttack, 0 }, { params::kAmpDecay, 100 },
        { params::kAmpSustain, 0.8f }, { params::kAmpRelease, 300 },
        { params::kSubOctave, kick.subOctave }, { params::kClickPunch, kick.clickPunch },
        { params::kStereoWidth, kick.width },
        { params::kHighPass, 20.0f }, { params::kLowPass, 20000.0f }
    };
    appendLayer(values, 0, LayerPreset{ true, DrumSoundType::kick, 1.0f, 0.0f, 300.0f, 20.0f, 20000.0f });
    appendLayer(values, 1, kick.layerB);
    appendLayer(values, 2, kick.layerC);
    appendLayer(values, 3, none);
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
