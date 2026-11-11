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
        // Keep the original 12-pattern wrap so programs saved against those indexes
        // still land on the same fill when the list grows past 12.
        const float slotPreset = static_cast<float>((static_cast<int>(basePresetIdx) + slot) % 12);
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
        } },
        { "Sub Floor (F0)", {
            { params::kNote, 5.0f }, { params::kOctave, 0.0f }, { params::kStartPitch, 180.0f },
            { params::kPitchDecay, 90.0f }, { params::kTailLength, 720.0f }, { params::kTailCurve, 0.05f },
            { params::kClickLevel, 0.08f }, { params::kPunch, 0.2f }, { params::kSubDrive, 0.15f }
        } },
        { "Clicky C2", {
            { params::kNote, 0.0f }, { params::kOctave, 2.0f }, { params::kStartPitch, 1400.0f },
            { params::kPitchDecay, 18.0f }, { params::kTailLength, 140.0f }, { params::kClickLevel, 0.9f },
            { params::kClickFreq, 6200.0f }, { params::kPunch, 0.85f }, { params::kSubDrive, 0.1f }
        } },
        { "Short Wire Snare (D1)", {
            { params::kNote, 2.0f }, { params::kTailLength, 220.0f }, { params::kClickLevel, 0.4f },
            { params::kSnareLevel, 0.92f }, { params::kSnareTone, 0.92f }, { params::kSnareDecay, 0.04f }
        } },
        { "Open Hat Aftertaste (A1)", {
            { params::kNote, 9.0f }, { params::kTailLength, 80.0f }, { params::kPunch, 0.6f },
            { params::kHatLevel, 0.9f }, { params::kHatTone, 0.82f }, { params::kHatDecay, 1.0f }
        } },
        { "Long Room Clap (E1)", {
            { params::kNote, 4.0f }, { params::kTailLength, 90.0f }, { params::kClickLevel, 0.25f },
            { params::kClapLevel, 0.88f }, { params::kClapSpread, 0.72f }, { params::kClapTone, 0.48f },
            { params::kClapDecay, 1.0f }
        } },
        { "Closed Hat Chip (G1)", {
            { params::kNote, 7.0f }, { params::kTailLength, 160.0f }, { params::kClickLevel, 0.5f },
            { params::kHatLevel, 0.75f }, { params::kHatTone, 1.0f }, { params::kHatDecay, 0.0f }
        } },
        { "Driven F#2", {
            { params::kNote, 6.0f }, { params::kOctave, 2.0f }, { params::kStartPitch, 980.0f },
            { params::kPitchDecay, 26.0f }, { params::kTailLength, 200.0f }, { params::kSubDrive, 0.95f },
            { params::kDrive, 0.72f }, { params::kPunch, 0.7f }, { params::kClickLevel, 0.55f }
        } },
        { "Low Passed Sub (C1)", {
            { params::kNote, 0.0f }, { params::kStartPitch, 240.0f }, { params::kPitchDecay, 70.0f },
            { params::kTailLength, 640.0f }, { params::kClickLevel, 0.12f }, { params::kPunch, 0.25f },
            { params::kLowPass, 180.0f }, { params::kDrive, 0.1f }
        } },
        { "High Passed Tick (B1)", {
            { params::kNote, 11.0f }, { params::kStartPitch, 760.0f }, { params::kPitchDecay, 16.0f },
            { params::kTailLength, 110.0f }, { params::kClickLevel, 0.85f }, { params::kClickFreq, 7400.0f },
            { params::kHighPass, 280.0f }, { params::kPunch, 0.4f }
        } },
        { "Half Beat Reverse (D1)", {
            { params::kNote, 2.0f }, { params::kTailLength, 360.0f }, { params::kReverseMode, 3.0f },
            { params::kReverseMix, 0.85f }, { params::kDrive, 0.2f }, { params::kPunch, 0.5f }
        } },
        { "Quarter Note Reverse (G#1)", {
            { params::kNote, 8.0f }, { params::kTailLength, 240.0f }, { params::kReverseMode, 4.0f },
            { params::kReverseMix, 1.0f }, { params::kClickLevel, 0.6f }, { params::kStartPitch, 640.0f }
        } },
        { "Long Dry Sine (F1)", {
            { params::kNote, 5.0f }, { params::kStartPitch, 90.0f }, { params::kPitchDecay, 8.0f },
            { params::kTailLength, 980.0f }, { params::kTailCurve, -0.35f }, { params::kClickLevel, 0.0f },
            { params::kPunch, 0.0f }, { params::kSubDrive, 0.0f }, { params::kDrive, 0.0f }
        } },
        { "Full Layer Stack (A#1)", {
            { params::kNote, 10.0f }, { params::kTailLength, 260.0f }, { params::kPunch, 0.55f },
            { params::kSnareLevel, 0.7f }, { params::kSnareTone, 0.4f }, { params::kSnareDecay, 0.85f },
            { params::kClapLevel, 0.62f }, { params::kClapSpread, 0.4f }, { params::kClapTone, 0.75f },
            { params::kClapDecay, 0.8f },
            { params::kHatLevel, 0.55f }, { params::kHatTone, 0.6f }, { params::kHatDecay, 0.7f }
        } },
        { "138 Uplifting Euphoria (F1)", {
            { params::kNote, 5.0f }, { params::kStartPitch, 480.0f }, { params::kPitchDecay, 34.0f },
            { params::kTailLength, 350.0f }, { params::kPunch, 0.68f }, { params::kSubDrive, 0.35f },
            { params::kClickLevel, 0.45f }, { params::kClickFreq, 3400.0f }, { params::kDrive, 0.20f } } },
        { "138 Pure Trance Slammer (G1)", {
            { params::kNote, 7.0f }, { params::kStartPitch, 520.0f }, { params::kPitchDecay, 32.0f },
            { params::kTailLength, 360.0f }, { params::kPunch, 0.72f }, { params::kSubDrive, 0.38f },
            { params::kClickLevel, 0.48f }, { params::kClickFreq, 3600.0f }, { params::kDrive, 0.22f } } },
        { "Trance Anthem Driver (A1)", {
            { params::kNote, 9.0f }, { params::kStartPitch, 460.0f }, { params::kPitchDecay, 35.0f },
            { params::kTailLength, 340.0f }, { params::kPunch, 0.75f }, { params::kSubDrive, 0.32f },
            { params::kClickLevel, 0.42f }, { params::kClickFreq, 4000.0f }, { params::kDrive, 0.18f } } },
        { "Psytrance Woody Tok (F#1)", {
            { params::kNote, 6.0f }, { params::kStartPitch, 580.0f }, { params::kPitchDecay, 22.0f },
            { params::kTailLength, 220.0f }, { params::kTailCurve, -0.22f }, { params::kPunch, 0.72f },
            { params::kClickLevel, 0.58f }, { params::kClickFreq, 4800.0f }, { params::kSubDrive, 0.28f } } },
        { "Psytrance Deep Forest Stomp (E1)", {
            { params::kNote, 4.0f }, { params::kStartPitch, 620.0f }, { params::kPitchDecay, 24.0f },
            { params::kTailLength, 240.0f }, { params::kTailCurve, -0.18f }, { params::kPunch, 0.70f },
            { params::kClickLevel, 0.52f }, { params::kClickFreq, 5200.0f }, { params::kSubDrive, 0.35f },
            { params::kDrive, 0.25f } } },
        { "Tech-Trance Hard Hitter (A#1)", {
            { params::kNote, 10.0f }, { params::kStartPitch, 680.0f }, { params::kPitchDecay, 42.0f },
            { params::kTailLength, 380.0f }, { params::kPunch, 0.78f }, { params::kSubDrive, 0.55f },
            { params::kClickLevel, 0.50f }, { params::kClickFreq, 3200.0f }, { params::kDrive, 0.35f } } },
        { "Tech-Trance Distorted Tok (G#1)", {
            { params::kNote, 8.0f }, { params::kStartPitch, 750.0f }, { params::kPitchDecay, 45.0f },
            { params::kTailLength, 390.0f }, { params::kPunch, 0.76f }, { params::kSubDrive, 0.62f },
            { params::kClickLevel, 0.60f }, { params::kClickFreq, 3800.0f }, { params::kDrive, 0.42f },
            { params::kCrushRate, 7200.0f }, { params::kCrushBits, 10.0f }, { params::kCrushMix, 0.22f } } },
        { "Progressive Trance Velvet Sub (D1)", {
            { params::kNote, 2.0f }, { params::kStartPitch, 320.0f }, { params::kPitchDecay, 48.0f },
            { params::kTailLength, 420.0f }, { params::kTailCurve, 0.25f }, { params::kPunch, 0.48f },
            { params::kSubDrive, 0.22f }, { params::kClickLevel, 0.25f }, { params::kClickFreq, 2200.0f },
            { params::kDrive, 0.12f } } },
        { "Progressive Trance Deep Click (C1)", {
            { params::kNote, 0.0f }, { params::kStartPitch, 300.0f }, { params::kPitchDecay, 50.0f },
            { params::kTailLength, 440.0f }, { params::kTailCurve, 0.20f }, { params::kPunch, 0.52f },
            { params::kSubDrive, 0.30f }, { params::kClickLevel, 0.40f }, { params::kClickFreq, 2600.0f } } },
        { "Classic 1999 Trance 909 (F1)", {
            { params::kNote, 5.0f }, { params::kStartPitch, 380.0f }, { params::kPitchDecay, 38.0f },
            { params::kTailLength, 390.0f }, { params::kTailCurve, 0.10f }, { params::kPunch, 0.58f },
            { params::kSubDrive, 0.26f }, { params::kClickLevel, 0.36f }, { params::kClickFreq, 2800.0f },
            { params::kDrive, 0.16f } } },
        { "Berlin Gate Runner (G1)", {
            { params::kNote, 7.0f }, { params::kTailLength, 400.0f }, { params::kGateRate, 2.0f },
            { params::kGateMix, 0.68f }, { params::kPunch, 0.65f }, { params::kSubDrive, 0.32f },
            { params::kDrive, 0.24f } } },
        { "Euphoric 138 Stutter Drop (A1)", {
            { params::kNote, 9.0f }, { params::kTailLength, 380.0f }, { params::kStutterRate, 2.0f },
            { params::kStutterMix, 0.65f }, { params::kPunch, 0.70f }, { params::kDrive, 0.26f } } },
        { "Hard Trance Reverse Bass Tok (D#1)", {
            { params::kNote, 3.0f }, { params::kTailLength, 440.0f }, { params::kReverseMode, 2.0f },
            { params::kReverseMix, 1.0f }, { params::kPunch, 0.72f }, { params::kSubDrive, 0.40f },
            { params::kDrive, 0.30f } } },
        { "Hard Trance Reverse Beat Sync (C#1)", {
            { params::kNote, 1.0f }, { params::kTailLength, 430.0f }, { params::kReverseMode, 1.0f },
            { params::kReverseMix, 1.0f }, { params::kPunch, 0.74f }, { params::kSubDrive, 0.44f },
            { params::kDrive, 0.32f } } },
        { "Celestial Euphoric Sub (B1)", {
            { params::kNote, 11.0f }, { params::kStartPitch, 460.0f }, { params::kPitchDecay, 36.0f },
            { params::kTailLength, 370.0f }, { params::kPunch, 0.66f }, { params::kSubDrive, 0.30f },
            { params::kClickLevel, 0.44f }, { params::kClickFreq, 3600.0f }, { params::kDrive, 0.20f } } },
        { "Vocal Trance Sidechain Thump (F1)", {
            { params::kNote, 5.0f }, { params::kTailLength, 360.0f }, { params::kPumpRate, 0.0f },
            { params::kPumpMix, 0.32f }, { params::kPunch, 0.65f }, { params::kSubDrive, 0.28f },
            { params::kDrive, 0.18f } } },
        { "138 BPM Rolling Offbeat Lock (G1)", {
            { params::kNote, 7.0f }, { params::kStartPitch, 500.0f }, { params::kPitchDecay, 30.0f },
            { params::kTailLength, 320.0f }, { params::kTailCurve, -0.15f }, { params::kPunch, 0.72f },
            { params::kClickLevel, 0.52f }, { params::kClickFreq, 4200.0f }, { params::kLockIn, 1.0f } } },
        { "Ibiza Sunset Progressive Driver (A1)", {
            { params::kNote, 9.0f }, { params::kStartPitch, 360.0f }, { params::kPitchDecay, 44.0f },
            { params::kTailLength, 380.0f }, { params::kPunch, 0.55f }, { params::kSubDrive, 0.24f },
            { params::kClickLevel, 0.32f }, { params::kClickFreq, 2800.0f }, { params::kDrive, 0.16f } } },
        makeMultiSlotBuildPreset("Trance 138 Build Riser (8 Bar)", 5.0f, 360.0f, 3.0f, 1.0f, 0.75f, 7.0f, 0.60f, 0.28f),
        makeMultiSlotBuildPreset("Trance Uplifting Odyssey (16 Bar)", 7.0f, 380.0f, 4.0f, 2.0f, 0.78f, 10.0f, 0.70f, 0.30f),
        makeMultiSlotBuildPreset("Trance Epic Arena Ascent (32 Bar)", 9.0f, 400.0f, 5.0f, 4.0f, 0.82f, 12.0f, 0.80f, 0.34f),
        makeMultiSlotBuildPreset("Trance 1/64 Machine Rush (4 Bar)", 2.0f, 340.0f, 2.0f, 11.0f, 0.85f, 6.0f, 0.50f, 0.26f),
        { "138 Trance Kick + Offbeat Hat (F1)", {
            { params::kNote, 5.0f }, { params::kTailLength, 360.0f }, { params::kPunch, 0.65f },
            { params::kSubDrive, 0.30f }, { params::kHatLevel, 0.44f }, { params::kHatTone, 0.80f },
            { params::kHatDecay, 0.32f } } },
        { "138 Trance Kick + Snare Clap Stack (G1)", {
            { params::kNote, 7.0f }, { params::kTailLength, 370.0f }, { params::kPunch, 0.68f },
            { params::kSubDrive, 0.34f }, { params::kSnareLevel, 0.50f }, { params::kSnareTone, 0.65f },
            { params::kSnareDecay, 0.40f }, { params::kClapLevel, 0.42f }, { params::kClapSpread, 0.50f },
            { params::kClapTone, 0.68f } } },
        { "Festival Mainstage Drop (G1)", {
            { params::kNote, 7.0f }, { params::kStartPitch, 540.0f }, { params::kPitchDecay, 45.0f },
            { params::kTailLength, 420.0f }, { params::kTailCurve, 0.25f }, { params::kPunch, 0.75f },
            { params::kSubDrive, 0.45f }, { params::kDrive, 0.35f }, { params::kClickLevel, 0.65f },
            { params::kClickFreq, 4500.0f } } },
        { "Melodic Techno Low End (F1)", {
            { params::kNote, 5.0f }, { params::kStartPitch, 380.0f }, { params::kPitchDecay, 36.0f },
            { params::kTailLength, 340.0f }, { params::kTailCurve, -0.1f }, { params::kPunch, 0.5f },
            { params::kSubDrive, 0.25f }, { params::kClickLevel, 0.3f }, { params::kClickFreq, 3200.0f },
            { params::kLowPass, 220.0f } } },
        { "Future Bass Air Cannon (D#1)", {
            { params::kNote, 3.0f }, { params::kStartPitch, 490.0f }, { params::kPitchDecay, 42.0f },
            { params::kTailLength, 510.0f }, { params::kTailCurve, 0.3f }, { params::kPunch, 0.65f },
            { params::kSubDrive, 0.5f }, { params::kSnareLevel, 0.35f }, { params::kSnareTone, 0.5f },
            { params::kHatLevel, 0.25f }, { params::kHatDecay, 0.4f } } },
        { "Psytrance Triplet Click (E1)", {
            { params::kNote, 4.0f }, { params::kStartPitch, 720.0f }, { params::kPitchDecay, 22.0f },
            { params::kTailLength, 160.0f }, { params::kTailCurve, -0.4f }, { params::kPunch, 0.85f },
            { params::kClickLevel, 0.9f }, { params::kClickFreq, 6500.0f }, { params::kHighPass, 45.0f } } },
        { "Hardstyle Screamer Tok (A1)", {
            { params::kNote, 9.0f }, { params::kStartPitch, 980.0f }, { params::kPitchDecay, 68.0f },
            { params::kTailLength, 480.0f }, { params::kTailCurve, 0.4f }, { params::kPunch, 0.8f },
            { params::kSubDrive, 0.9f }, { params::kDrive, 0.75f }, { params::kClickLevel, 0.85f },
            { params::kClickFreq, 5800.0f } } },
        { "Bass House Thump & Layer (F#1)", {
            { params::kNote, 6.0f }, { params::kStartPitch, 460.0f }, { params::kPitchDecay, 38.0f },
            { params::kTailLength, 290.0f }, { params::kTailCurve, 0.05f }, { params::kPunch, 0.7f },
            { params::kSubDrive, 0.4f }, { params::kClapLevel, 0.55f }, { params::kClapSpread, 0.45f },
            { params::kClapTone, 0.7f }, { params::kClapDecay, 0.35f } } },
        { "Deep Tech Rolling Thud (C1)", {
            { params::kNote, 0.0f }, { params::kStartPitch, 310.0f }, { params::kPitchDecay, 30.0f },
            { params::kTailLength, 380.0f }, { params::kTailCurve, -0.2f }, { params::kPunch, 0.45f },
            { params::kClickLevel, 0.2f }, { params::kClickFreq, 2800.0f }, { params::kSubDrive, 0.35f } } },
        { "Slap House Knock (A#1)", {
            { params::kNote, 10.0f }, { params::kStartPitch, 580.0f }, { params::kPitchDecay, 26.0f },
            { params::kTailLength, 210.0f }, { params::kTailCurve, -0.15f }, { params::kPunch, 0.8f },
            { params::kSubDrive, 0.3f }, { params::kClickLevel, 0.75f }, { params::kClickFreq, 5200.0f },
            { params::kSnareLevel, 0.25f }, { params::kSnareDecay, 0.15f } } },
        { "Dubstep Heavy Heavy Weight (D1)", {
            { params::kNote, 2.0f }, { params::kStartPitch, 640.0f }, { params::kPitchDecay, 52.0f },
            { params::kTailLength, 560.0f }, { params::kTailCurve, 0.35f }, { params::kPunch, 0.7f },
            { params::kSubDrive, 0.7f }, { params::kDrive, 0.45f }, { params::kCrushRate, 4200.0f },
            { params::kCrushBits, 7.0f }, { params::kCrushMix, 0.35f } } },
        { "Progressive Trance Driving Sub (B1)", {
            { params::kNote, 11.0f }, { params::kStartPitch, 420.0f }, { params::kPitchDecay, 34.0f },
            { params::kTailLength, 440.0f }, { params::kTailCurve, 0.15f }, { params::kPunch, 0.6f },
            { params::kSubDrive, 0.35f }, { params::kDrive, 0.2f }, { params::kHatLevel, 0.3f },
            { params::kHatDecay, 0.25f } } },
        { "Electro Punk Raw Smasher (G#1)", {
            { params::kNote, 8.0f }, { params::kStartPitch, 820.0f }, { params::kPitchDecay, 48.0f },
            { params::kTailLength, 360.0f }, { params::kTailCurve, 0.2f }, { params::kPunch, 0.75f },
            { params::kSubDrive, 0.8f }, { params::kDrive, 0.65f }, { params::kClickLevel, 0.7f },
            { params::kClapLevel, 0.45f }, { params::kClapTone, 0.8f } } },
        { "DnB Rolling Sub 174 (C#1)", {
            { params::kNote, 1.0f }, { params::kStartPitch, 450.0f }, { params::kPitchDecay, 24.0f },
            { params::kTailLength, 180.0f }, { params::kTailCurve, -0.3f }, { params::kPunch, 0.85f },
            { params::kClickLevel, 0.6f }, { params::kClickFreq, 4800.0f }, { params::kSubDrive, 0.35f } } }
    };
    return presets;
}

const std::vector<FactoryPreset>& fillBuildPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> list;
        const std::array<float, core::kNumFillPresets> defaultShorten{
            0.55f, 0.68f, 0.82f, 0.50f, 0.76f, 0.70f, 0.58f, 0.72f, 0.45f, 0.80f, 0.78f, 0.88f,
            0.62f, 0.70f, 0.48f, 0.74f,
            0.65f, 0.75f, 0.80f, 0.60f, 0.70f, 0.50f, 0.85f, 0.62f, 0.72f, 0.78f, 0.55f, 0.82f
        };
        const std::array<float, core::kNumFillPresets> defaultPitchRise{
            0.0f, 4.0f, 7.0f, 0.0f, 6.0f, 12.0f, 2.0f, 5.0f, 0.0f, 8.0f, 5.0f, 9.0f,
            3.0f, 6.0f, 1.0f, 8.0f,
            4.0f, 7.0f, 12.0f, 3.0f, 5.0f, 0.0f, 12.0f, 2.0f, 7.0f, 6.0f, 2.0f, 8.0f
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
