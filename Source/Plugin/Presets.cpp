#include "Presets.h"
#include "Parameters.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>

namespace openedmkick::presets {

namespace {

struct TranceKick {
    const char* name;
    int note = 5;
    int octave = 1;
    float startPitchHz = 480.0f;
    float pitchDecayMs = 34.0f;
    float pitchCurve = 0.25f;
    float tailLengthMs = 350.0f;
    float tailCurve = 0.15f;
    float bodyTone = 0.65f;
    float attackMs = 0.35f;
    float decayMs = 72.0f;
    float sustainLevel = 0.7f;
    float releaseMs = 180.0f;
    float punch = 0.7f;
    float subDrive = 0.35f;
    float clickLevel = 0.6f;
    float clickFreqHz = 4000.0f;
    float snareLevel = 0.0f;
    float snareTone = 0.55f;
    float snareDecay = 0.4f;
    float clapLevel = 0.0f;
    float clapSpread = 0.45f;
    float clapTone = 0.6f;
    float clapDecay = 0.35f;
    float hatLevel = 0.0f;
    float hatTone = 0.7f;
    float hatDecay = 0.25f;
    float drive = 0.15f;
    float gateMix = 0.0f;
    int reverseMode = 0;
    float reverseMix = 1.0f;
    float crushMix = 0.0f;
};

const std::array<TranceKick, 48> kTranceKicks{{
    { .name = "Northstar 138", .note = 5, .startPitchHz = 760.0f, .pitchDecayMs = 30.0f, .pitchCurve = 0.42f, .tailLengthMs = 330.0f, .tailCurve = 0.18f, .bodyTone = 0.78f, .attackMs = 0.18f, .decayMs = 48.0f, .sustainLevel = 0.62f, .releaseMs = 155.0f, .punch = 0.86f, .subDrive = 0.42f, .clickLevel = 0.72f, .clickFreqHz = 4600.0f },
    { .name = "Violet Horizon", .note = 7, .startPitchHz = 720.0f, .pitchDecayMs = 33.0f, .pitchCurve = 0.36f, .tailLengthMs = 345.0f, .tailCurve = 0.24f, .bodyTone = 0.84f, .attackMs = 0.22f, .decayMs = 64.0f, .sustainLevel = 0.56f, .releaseMs = 168.0f, .punch = 0.82f, .subDrive = 0.48f, .clickLevel = 0.66f, .clickFreqHz = 5100.0f, .hatLevel = 0.12f, .hatTone = 0.76f, .hatDecay = 0.08f },
    { .name = "Euphoric Striker", .note = 9, .startPitchHz = 810.0f, .pitchDecayMs = 28.0f, .pitchCurve = 0.5f, .tailLengthMs = 310.0f, .tailCurve = 0.12f, .bodyTone = 0.9f, .attackMs = 0.14f, .decayMs = 38.0f, .sustainLevel = 0.54f, .releaseMs = 140.0f, .punch = 0.94f, .subDrive = 0.56f, .clickLevel = 0.78f, .clickFreqHz = 5700.0f, .snareLevel = 0.08f, .snareTone = 0.55f, .snareDecay = 0.18f },
    { .name = "Stadium Bloom", .note = 5, .startPitchHz = 680.0f, .pitchDecayMs = 39.0f, .pitchCurve = 0.28f, .tailLengthMs = 390.0f, .tailCurve = 0.3f, .bodyTone = 0.72f, .attackMs = 0.28f, .decayMs = 96.0f, .sustainLevel = 0.67f, .releaseMs = 215.0f, .punch = 0.79f, .subDrive = 0.4f, .clickLevel = 0.6f, .clickFreqHz = 3900.0f, .clapLevel = 0.12f, .clapSpread = 0.18f, .clapTone = 0.58f, .clapDecay = 0.28f },
    { .name = "Rise Through Light", .note = 2, .startPitchHz = 790.0f, .pitchDecayMs = 35.0f, .pitchCurve = 0.55f, .tailLengthMs = 335.0f, .tailCurve = 0.08f, .bodyTone = 0.82f, .attackMs = 0.12f, .decayMs = 58.0f, .sustainLevel = 0.6f, .releaseMs = 172.0f, .punch = 0.9f, .subDrive = 0.5f, .clickLevel = 0.73f, .clickFreqHz = 5300.0f },
    { .name = "Eternal Gate", .note = 7, .startPitchHz = 640.0f, .pitchDecayMs = 44.0f, .pitchCurve = 0.22f, .tailLengthMs = 420.0f, .tailCurve = 0.34f, .bodyTone = 0.68f, .attackMs = 0.42f, .decayMs = 120.0f, .sustainLevel = 0.68f, .releaseMs = 240.0f, .punch = 0.77f, .subDrive = 0.38f, .clickLevel = 0.54f, .clickFreqHz = 3700.0f, .drive = 0.27f },
    { .name = "Skybreaker 140", .note = 4, .startPitchHz = 880.0f, .pitchDecayMs = 27.0f, .pitchCurve = 0.6f, .tailLengthMs = 305.0f, .tailCurve = 0.06f, .bodyTone = 0.96f, .attackMs = 0.1f, .decayMs = 33.0f, .sustainLevel = 0.52f, .releaseMs = 126.0f, .punch = 0.98f, .subDrive = 0.58f, .clickLevel = 0.83f, .clickFreqHz = 6400.0f },
    { .name = "Aether Drive", .note = 11, .startPitchHz = 700.0f, .pitchDecayMs = 36.0f, .pitchCurve = 0.3f, .tailLengthMs = 360.0f, .tailCurve = 0.16f, .bodyTone = 0.74f, .attackMs = 0.26f, .decayMs = 80.0f, .sustainLevel = 0.64f, .releaseMs = 190.0f, .punch = 0.8f, .subDrive = 0.44f, .clickLevel = 0.62f, .clickFreqHz = 4300.0f, .hatLevel = 0.1f, .hatTone = 0.84f, .hatDecay = 0.12f },
    { .name = "Solstice Anthem", .note = 0, .startPitchHz = 740.0f, .pitchDecayMs = 32.0f, .pitchCurve = 0.47f, .tailLengthMs = 350.0f, .tailCurve = 0.2f, .bodyTone = 0.86f, .attackMs = 0.2f, .decayMs = 69.0f, .sustainLevel = 0.6f, .releaseMs = 175.0f, .punch = 0.88f, .subDrive = 0.5f, .clickLevel = 0.74f, .clickFreqHz = 4900.0f, .drive = 0.24f },
    { .name = "Last Dawn 138", .note = 5, .startPitchHz = 620.0f, .pitchDecayMs = 47.0f, .pitchCurve = 0.2f, .tailLengthMs = 400.0f, .tailCurve = 0.38f, .bodyTone = 0.62f, .attackMs = 0.5f, .decayMs = 138.0f, .sustainLevel = 0.72f, .releaseMs = 238.0f, .punch = 0.7f, .subDrive = 0.3f, .clickLevel = 0.47f, .clickFreqHz = 3300.0f },
    { .name = "High Orbit Slam", .note = 2, .startPitchHz = 830.0f, .pitchDecayMs = 26.0f, .pitchCurve = 0.58f, .tailLengthMs = 315.0f, .tailCurve = 0.11f, .bodyTone = 0.92f, .attackMs = 0.12f, .decayMs = 42.0f, .sustainLevel = 0.53f, .releaseMs = 142.0f, .punch = 0.96f, .subDrive = 0.57f, .clickLevel = 0.81f, .clickFreqHz = 6100.0f },
    { .name = "Mainline Euphoria", .note = 9, .startPitchHz = 690.0f, .pitchDecayMs = 38.0f, .pitchCurve = 0.32f, .tailLengthMs = 365.0f, .tailCurve = 0.22f, .bodyTone = 0.76f, .attackMs = 0.24f, .decayMs = 88.0f, .sustainLevel = 0.65f, .releaseMs = 205.0f, .punch = 0.83f, .subDrive = 0.43f, .clickLevel = 0.64f, .clickFreqHz = 4200.0f, .clapLevel = 0.1f, .clapSpread = 0.12f, .clapTone = 0.62f, .clapDecay = 0.25f },

    { .name = "Sundial Trance", .note = 0, .startPitchHz = 610.0f, .pitchDecayMs = 42.0f, .pitchCurve = 0.16f, .tailLengthMs = 410.0f, .tailCurve = 0.16f, .bodyTone = 0.63f, .attackMs = 0.32f, .decayMs = 110.0f, .sustainLevel = 0.72f, .releaseMs = 225.0f, .punch = 0.7f, .subDrive = 0.31f, .clickLevel = 0.48f, .clickFreqHz = 3500.0f, .hatLevel = 0.12f, .hatTone = 0.65f, .hatDecay = 0.16f },
    { .name = "Orbit Assembly", .note = 4, .startPitchHz = 580.0f, .pitchDecayMs = 45.0f, .pitchCurve = 0.14f, .tailLengthMs = 395.0f, .tailCurve = 0.12f, .bodyTone = 0.6f, .attackMs = 0.3f, .decayMs = 100.0f, .sustainLevel = 0.69f, .releaseMs = 205.0f, .punch = 0.72f, .subDrive = 0.33f, .clickLevel = 0.47f, .clickFreqHz = 3450.0f },

    { .name = "Chrome Dawn 909", .note = 5, .startPitchHz = 650.0f, .pitchDecayMs = 36.0f, .pitchCurve = 0.28f, .tailLengthMs = 360.0f, .tailCurve = 0.14f, .bodyTone = 0.73f, .attackMs = 0.22f, .decayMs = 82.0f, .sustainLevel = 0.66f, .releaseMs = 188.0f, .punch = 0.8f, .subDrive = 0.38f, .clickLevel = 0.58f, .clickFreqHz = 4100.0f, .drive = 0.2f },
    { .name = "Rave Hall Vintage", .note = 7, .startPitchHz = 690.0f, .pitchDecayMs = 34.0f, .pitchCurve = 0.34f, .tailLengthMs = 350.0f, .tailCurve = 0.12f, .bodyTone = 0.8f, .attackMs = 0.24f, .decayMs = 74.0f, .sustainLevel = 0.62f, .releaseMs = 178.0f, .punch = 0.82f, .subDrive = 0.4f, .clickLevel = 0.61f, .clickFreqHz = 4400.0f },
    { .name = "Tape Echo 138", .note = 9, .startPitchHz = 610.0f, .pitchDecayMs = 39.0f, .pitchCurve = 0.24f, .tailLengthMs = 375.0f, .tailCurve = 0.18f, .bodyTone = 0.69f, .attackMs = 0.28f, .decayMs = 92.0f, .sustainLevel = 0.67f, .releaseMs = 195.0f, .punch = 0.76f, .subDrive = 0.36f, .clickLevel = 0.54f, .clickFreqHz = 3800.0f, .drive = 0.32f },
    { .name = "Iron Bloom 909", .note = 2, .startPitchHz = 740.0f, .pitchDecayMs = 31.0f, .pitchCurve = 0.42f, .tailLengthMs = 325.0f, .tailCurve = 0.09f, .bodyTone = 0.88f, .attackMs = 0.16f, .decayMs = 54.0f, .sustainLevel = 0.58f, .releaseMs = 148.0f, .punch = 0.9f, .subDrive = 0.51f, .clickLevel = 0.72f, .clickFreqHz = 5200.0f, .drive = 0.38f },
    { .name = "Neon Basement 132", .note = 0, .startPitchHz = 560.0f, .pitchDecayMs = 47.0f, .pitchCurve = 0.12f, .tailLengthMs = 410.0f, .tailCurve = 0.2f, .bodyTone = 0.58f, .attackMs = 0.4f, .decayMs = 116.0f, .sustainLevel = 0.73f, .releaseMs = 232.0f, .punch = 0.66f, .subDrive = 0.32f, .clickLevel = 0.45f, .clickFreqHz = 3150.0f },
    { .name = "Dry Room Driver", .note = 4, .startPitchHz = 820.0f, .pitchDecayMs = 24.0f, .pitchCurve = 0.64f, .tailLengthMs = 255.0f, .tailCurve = -0.06f, .bodyTone = 0.94f, .attackMs = 0.08f, .decayMs = 28.0f, .sustainLevel = 0.4f, .releaseMs = 105.0f, .punch = 0.96f, .subDrive = 0.55f, .clickLevel = 0.8f, .clickFreqHz = 6800.0f },
    { .name = "Glass Transistor", .note = 11, .startPitchHz = 780.0f, .pitchDecayMs = 28.0f, .pitchCurve = 0.58f, .tailLengthMs = 285.0f, .tailCurve = 0.04f, .bodyTone = 0.98f, .attackMs = 0.07f, .decayMs = 30.0f, .sustainLevel = 0.38f, .releaseMs = 112.0f, .punch = 0.99f, .subDrive = 0.62f, .clickLevel = 0.86f, .clickFreqHz = 7200.0f, .crushMix = 0.12f },
    { .name = "Analog Gate 138", .note = 5, .startPitchHz = 670.0f, .pitchDecayMs = 35.0f, .pitchCurve = 0.3f, .tailLengthMs = 345.0f, .tailCurve = 0.13f, .bodyTone = 0.77f, .attackMs = 0.2f, .decayMs = 72.0f, .sustainLevel = 0.62f, .releaseMs = 168.0f, .punch = 0.82f, .subDrive = 0.41f, .clickLevel = 0.62f, .clickFreqHz = 4550.0f, .drive = 0.22f, .gateMix = 0.3f },
    { .name = "Rotor Wash Anthem", .note = 7, .startPitchHz = 640.0f, .pitchDecayMs = 38.0f, .pitchCurve = 0.24f, .tailLengthMs = 370.0f, .tailCurve = 0.16f, .bodyTone = 0.71f, .attackMs = 0.26f, .decayMs = 88.0f, .sustainLevel = 0.66f, .releaseMs = 190.0f, .punch = 0.78f, .subDrive = 0.38f, .clickLevel = 0.56f, .clickFreqHz = 4000.0f },
    { .name = "Nightline 909", .note = 2, .startPitchHz = 700.0f, .pitchDecayMs = 33.0f, .pitchCurve = 0.35f, .tailLengthMs = 340.0f, .tailCurve = 0.1f, .bodyTone = 0.83f, .attackMs = 0.18f, .decayMs = 62.0f, .sustainLevel = 0.59f, .releaseMs = 162.0f, .punch = 0.86f, .subDrive = 0.46f, .clickLevel = 0.67f, .clickFreqHz = 5000.0f },
    { .name = "Chroma Drive", .note = 9, .startPitchHz = 760.0f, .pitchDecayMs = 29.0f, .pitchCurve = 0.5f, .tailLengthMs = 315.0f, .tailCurve = 0.08f, .bodyTone = 0.91f, .attackMs = 0.12f, .decayMs = 44.0f, .sustainLevel = 0.51f, .releaseMs = 140.0f, .punch = 0.92f, .subDrive = 0.54f, .clickLevel = 0.78f, .clickFreqHz = 5900.0f, .drive = 0.35f },
    { .name = "Mirrorball Impact", .note = 4, .startPitchHz = 690.0f, .pitchDecayMs = 34.0f, .pitchCurve = 0.32f, .tailLengthMs = 350.0f, .tailCurve = 0.14f, .bodyTone = 0.8f, .attackMs = 0.2f, .decayMs = 70.0f, .sustainLevel = 0.61f, .releaseMs = 172.0f, .punch = 0.85f, .subDrive = 0.43f, .clickLevel = 0.65f, .clickFreqHz = 4800.0f, .clapLevel = 0.12f, .clapSpread = 0.08f, .clapTone = 0.72f },
    { .name = "Voltage Parade", .note = 0, .startPitchHz = 730.0f, .pitchDecayMs = 32.0f, .pitchCurve = 0.39f, .tailLengthMs = 330.0f, .tailCurve = 0.1f, .bodyTone = 0.87f, .attackMs = 0.15f, .decayMs = 50.0f, .sustainLevel = 0.56f, .releaseMs = 150.0f, .punch = 0.9f, .subDrive = 0.5f, .clickLevel = 0.74f, .clickFreqHz = 5600.0f },

    { .name = "Forest Vector 145", .note = 4, .startPitchHz = 960.0f, .pitchDecayMs = 21.0f, .pitchCurve = 0.7f, .tailLengthMs = 250.0f, .tailCurve = -0.08f, .bodyTone = 1.0f, .attackMs = 0.06f, .decayMs = 24.0f, .sustainLevel = 0.31f, .releaseMs = 95.0f, .punch = 1.0f, .subDrive = 0.72f, .clickLevel = 0.95f, .clickFreqHz = 7800.0f, .drive = 0.46f },
    { .name = "Carbon Triplet", .note = 2, .startPitchHz = 880.0f, .pitchDecayMs = 24.0f, .pitchCurve = 0.65f, .tailLengthMs = 275.0f, .tailCurve = -0.04f, .bodyTone = 0.98f, .attackMs = 0.07f, .decayMs = 27.0f, .sustainLevel = 0.35f, .releaseMs = 105.0f, .punch = 0.98f, .subDrive = 0.68f, .clickLevel = 0.91f, .clickFreqHz = 7400.0f, .crushMix = 0.18f },
    { .name = "Spiral Resin", .note = 9, .startPitchHz = 920.0f, .pitchDecayMs = 22.0f, .pitchCurve = 0.68f, .tailLengthMs = 260.0f, .tailCurve = -0.06f, .bodyTone = 0.99f, .attackMs = 0.06f, .decayMs = 25.0f, .sustainLevel = 0.32f, .releaseMs = 100.0f, .punch = 0.99f, .subDrive = 0.7f, .clickLevel = 0.94f, .clickFreqHz = 7600.0f },
    { .name = "Red Dust Pulse", .note = 11, .startPitchHz = 860.0f, .pitchDecayMs = 25.0f, .pitchCurve = 0.62f, .tailLengthMs = 285.0f, .tailCurve = -0.02f, .bodyTone = 0.95f, .attackMs = 0.08f, .decayMs = 31.0f, .sustainLevel = 0.38f, .releaseMs = 112.0f, .punch = 0.96f, .subDrive = 0.66f, .clickLevel = 0.88f, .clickFreqHz = 7000.0f, .drive = 0.42f },
    { .name = "Psy Meridian 148", .note = 5, .startPitchHz = 980.0f, .pitchDecayMs = 20.0f, .pitchCurve = 0.74f, .tailLengthMs = 240.0f, .tailCurve = -0.1f, .bodyTone = 1.0f, .attackMs = 0.05f, .decayMs = 22.0f, .sustainLevel = 0.28f, .releaseMs = 90.0f, .punch = 1.0f, .subDrive = 0.74f, .clickLevel = 0.98f, .clickFreqHz = 8200.0f, .drive = 0.5f },
    { .name = "Wormhole 144", .note = 7, .startPitchHz = 900.0f, .pitchDecayMs = 23.0f, .pitchCurve = 0.66f, .tailLengthMs = 265.0f, .tailCurve = -0.04f, .bodyTone = 0.98f, .attackMs = 0.06f, .decayMs = 27.0f, .sustainLevel = 0.34f, .releaseMs = 102.0f, .punch = 0.98f, .subDrive = 0.69f, .clickLevel = 0.92f, .clickFreqHz = 7600.0f, .reverseMode = 3, .reverseMix = 0.42f },
    { .name = "Acid Lantern", .note = 0, .startPitchHz = 840.0f, .pitchDecayMs = 26.0f, .pitchCurve = 0.58f, .tailLengthMs = 300.0f, .tailCurve = 0.02f, .bodyTone = 0.93f, .attackMs = 0.1f, .decayMs = 35.0f, .sustainLevel = 0.42f, .releaseMs = 120.0f, .punch = 0.94f, .subDrive = 0.64f, .clickLevel = 0.86f, .clickFreqHz = 6800.0f, .drive = 0.47f, .crushMix = 0.1f },
    { .name = "Triad Runner", .note = 2, .startPitchHz = 930.0f, .pitchDecayMs = 21.0f, .pitchCurve = 0.72f, .tailLengthMs = 255.0f, .tailCurve = -0.07f, .bodyTone = 0.99f, .attackMs = 0.05f, .decayMs = 24.0f, .sustainLevel = 0.3f, .releaseMs = 94.0f, .punch = 1.0f, .subDrive = 0.71f, .clickLevel = 0.96f, .clickFreqHz = 7900.0f, .hatLevel = 0.14f, .hatTone = 0.95f, .hatDecay = 0.04f },
    { .name = "Moss Engine", .note = 9, .startPitchHz = 790.0f, .pitchDecayMs = 29.0f, .pitchCurve = 0.52f, .tailLengthMs = 315.0f, .tailCurve = 0.06f, .bodyTone = 0.89f, .attackMs = 0.12f, .decayMs = 48.0f, .sustainLevel = 0.49f, .releaseMs = 138.0f, .punch = 0.91f, .subDrive = 0.6f, .clickLevel = 0.79f, .clickFreqHz = 6200.0f },
    { .name = "Obsidian Needle", .note = 4, .startPitchHz = 1020.0f, .pitchDecayMs = 19.0f, .pitchCurve = 0.78f, .tailLengthMs = 230.0f, .tailCurve = -0.12f, .bodyTone = 1.0f, .attackMs = 0.05f, .decayMs = 20.0f, .sustainLevel = 0.25f, .releaseMs = 86.0f, .punch = 1.0f, .subDrive = 0.78f, .clickLevel = 1.0f, .clickFreqHz = 9000.0f, .crushMix = 0.15f },
    { .name = "Saffron Spiral", .note = 11, .startPitchHz = 870.0f, .pitchDecayMs = 25.0f, .pitchCurve = 0.61f, .tailLengthMs = 285.0f, .tailCurve = -0.01f, .bodyTone = 0.96f, .attackMs = 0.09f, .decayMs = 33.0f, .sustainLevel = 0.39f, .releaseMs = 116.0f, .punch = 0.95f, .subDrive = 0.67f, .clickLevel = 0.87f, .clickFreqHz = 7100.0f },
    { .name = "Feral Current", .note = 5, .startPitchHz = 950.0f, .pitchDecayMs = 20.0f, .pitchCurve = 0.75f, .tailLengthMs = 245.0f, .tailCurve = -0.1f, .bodyTone = 1.0f, .attackMs = 0.05f, .decayMs = 22.0f, .sustainLevel = 0.29f, .releaseMs = 92.0f, .punch = 1.0f, .subDrive = 0.73f, .clickLevel = 0.97f, .clickFreqHz = 8400.0f, .drive = 0.52f },
    { .name = "Amber Vortex", .note = 7, .startPitchHz = 900.0f, .pitchDecayMs = 23.0f, .pitchCurve = 0.67f, .tailLengthMs = 270.0f, .tailCurve = -0.03f, .bodyTone = 0.98f, .attackMs = 0.06f, .decayMs = 27.0f, .sustainLevel = 0.35f, .releaseMs = 104.0f, .punch = 0.98f, .subDrive = 0.7f, .clickLevel = 0.92f, .clickFreqHz = 7700.0f, .reverseMode = 4, .reverseMix = 0.35f },

    { .name = "Hard Circuit 145", .note = 5, .startPitchHz = 900.0f, .pitchDecayMs = 23.0f, .pitchCurve = 0.68f, .tailLengthMs = 290.0f, .tailCurve = 0.04f, .bodyTone = 0.97f, .attackMs = 0.07f, .decayMs = 30.0f, .sustainLevel = 0.37f, .releaseMs = 110.0f, .punch = 0.98f, .subDrive = 0.76f, .clickLevel = 0.92f, .clickFreqHz = 7700.0f, .drive = 0.54f },
    { .name = "Voltage Cathedral", .note = 2, .startPitchHz = 820.0f, .pitchDecayMs = 28.0f, .pitchCurve = 0.57f, .tailLengthMs = 325.0f, .tailCurve = 0.08f, .bodyTone = 0.91f, .attackMs = 0.1f, .decayMs = 42.0f, .sustainLevel = 0.48f, .releaseMs = 135.0f, .punch = 0.93f, .subDrive = 0.68f, .clickLevel = 0.86f, .clickFreqHz = 6900.0f, .drive = 0.45f },
    { .name = "Steel Anthem 142", .note = 9, .startPitchHz = 850.0f, .pitchDecayMs = 26.0f, .pitchCurve = 0.6f, .tailLengthMs = 310.0f, .tailCurve = 0.06f, .bodyTone = 0.94f, .attackMs = 0.09f, .decayMs = 38.0f, .sustainLevel = 0.44f, .releaseMs = 128.0f, .punch = 0.95f, .subDrive = 0.7f, .clickLevel = 0.89f, .clickFreqHz = 7200.0f, .clapLevel = 0.08f, .clapSpread = 0.06f, .clapTone = 0.76f },
    { .name = "Front Row Impact", .note = 0, .startPitchHz = 790.0f, .pitchDecayMs = 30.0f, .pitchCurve = 0.52f, .tailLengthMs = 340.0f, .tailCurve = 0.1f, .bodyTone = 0.88f, .attackMs = 0.13f, .decayMs = 53.0f, .sustainLevel = 0.54f, .releaseMs = 155.0f, .punch = 0.91f, .subDrive = 0.63f, .clickLevel = 0.82f, .clickFreqHz = 6400.0f, .drive = 0.4f },
    { .name = "Burner 150", .note = 4, .startPitchHz = 1060.0f, .pitchDecayMs = 18.0f, .pitchCurve = 0.82f, .tailLengthMs = 220.0f, .tailCurve = -0.14f, .bodyTone = 1.0f, .attackMs = 0.05f, .decayMs = 18.0f, .sustainLevel = 0.23f, .releaseMs = 82.0f, .punch = 1.0f, .subDrive = 0.82f, .clickLevel = 1.0f, .clickFreqHz = 9800.0f, .drive = 0.62f, .crushMix = 0.2f },
    { .name = "Edge of Orbit", .note = 11, .startPitchHz = 880.0f, .pitchDecayMs = 24.0f, .pitchCurve = 0.64f, .tailLengthMs = 300.0f, .tailCurve = 0.02f, .bodyTone = 0.95f, .attackMs = 0.08f, .decayMs = 34.0f, .sustainLevel = 0.4f, .releaseMs = 118.0f, .punch = 0.96f, .subDrive = 0.72f, .clickLevel = 0.9f, .clickFreqHz = 7500.0f, .hatLevel = 0.1f, .hatTone = 0.9f, .hatDecay = 0.05f },
    { .name = "Peak Signal 138", .note = 5, .startPitchHz = 740.0f, .pitchDecayMs = 32.0f, .pitchCurve = 0.42f, .tailLengthMs = 355.0f, .tailCurve = 0.16f, .bodyTone = 0.83f, .attackMs = 0.19f, .decayMs = 68.0f, .sustainLevel = 0.6f, .releaseMs = 180.0f, .punch = 0.87f, .subDrive = 0.51f, .clickLevel = 0.72f, .clickFreqHz = 5200.0f, .drive = 0.28f },
    { .name = "Final Beacon", .note = 7, .startPitchHz = 930.0f, .pitchDecayMs = 22.0f, .pitchCurve = 0.72f, .tailLengthMs = 270.0f, .tailCurve = -0.05f, .bodyTone = 0.99f, .attackMs = 0.06f, .decayMs = 25.0f, .sustainLevel = 0.32f, .releaseMs = 100.0f, .punch = 0.99f, .subDrive = 0.74f, .clickLevel = 0.95f, .clickFreqHz = 8100.0f, .drive = 0.48f, .reverseMode = 2, .reverseMix = 0.38f }
}};

[[nodiscard]] bool sectionLocked(uint16_t mask, params::Section section) noexcept {
    return (mask & static_cast<uint16_t>(1u << static_cast<int>(section))) != 0;
}

[[nodiscard]] params::Section sectionForParameter(const juce::String& id) noexcept {
    if (id == params::kNote || id == params::kMidiMode || id == params::kOctave || id == params::kFineTune) {
        return params::Section::tuning;
    }
    if (id == params::kStartPitch || id == params::kPitchDecay || id == params::kPitchCurve) {
        return params::Section::pitch;
    }
    if (id == params::kTailLength || id == params::kTailCurve || id == params::kBodyTone
        || id == params::kAmpAttack || id == params::kAmpDecay || id == params::kAmpSustain
        || id == params::kAmpRelease || id == params::kSubDrive || id == params::kPunch
        || id == params::kLockIn) {
        return params::Section::bodyEnvelope;
    }
    if (id == params::kClickLevel || id == params::kClickFreq) {
        return params::Section::transient;
    }
    if (id == params::kSnareLevel || id == params::kSnareTone || id == params::kSnareDecay
        || id == params::kClapLevel || id == params::kClapSpread || id == params::kClapTone
        || id == params::kClapDecay || id == params::kHatLevel || id == params::kHatTone
        || id == params::kHatDecay) {
        return params::Section::layers;
    }
    if (id.startsWith("auto_") || id == params::kClockSync || id == params::kTempoBpm
        || id == params::kTimeSignature) {
        return params::Section::autoMode;
    }
    if (id == params::kReverseMode || id == params::kReverseMix) {
        return params::Section::reverse;
    }
    if (id == params::kDrive || id == params::kCrushRate || id == params::kCrushBits || id == params::kCrushMix) {
        return params::Section::texture;
    }
    if (id == params::kStutterRate || id == params::kStutterMix || id == params::kGateRate
        || id == params::kGateMix || id == params::kPumpRate || id == params::kPumpMix) {
        return params::Section::rhythm;
    }
    return params::Section::toneOutput;
}

[[nodiscard]] bool isLockedParameter(uint16_t mask, const juce::String& id) noexcept {
    return sectionLocked(mask, sectionForParameter(id));
}

[[nodiscard]] FactoryPreset makeFactoryPreset(const TranceKick& kick) {
    FactoryPreset preset{ kick.name, {
        { params::kNote, static_cast<float>(kick.note) },
        { params::kOctave, static_cast<float>(kick.octave) },
        { params::kFineTune, 0.0f },
        { params::kStartPitch, kick.startPitchHz },
        { params::kPitchDecay, kick.pitchDecayMs },
        { params::kPitchCurve, kick.pitchCurve },
        { params::kTailLength, kick.tailLengthMs },
        { params::kTailCurve, kick.tailCurve },
        { params::kBodyTone, kick.bodyTone },
        { params::kAmpAttack, kick.attackMs },
        { params::kAmpDecay, kick.decayMs },
        { params::kAmpSustain, kick.sustainLevel },
        { params::kAmpRelease, kick.releaseMs },
        { params::kPunch, kick.punch },
        { params::kSubDrive, kick.subDrive },
        { params::kClickLevel, kick.clickLevel },
        { params::kClickFreq, kick.clickFreqHz },
        { params::kSnareLevel, kick.snareLevel },
        { params::kSnareTone, kick.snareTone },
        { params::kSnareDecay, kick.snareDecay },
        { params::kClapLevel, kick.clapLevel },
        { params::kClapSpread, kick.clapSpread },
        { params::kClapTone, kick.clapTone },
        { params::kClapDecay, kick.clapDecay },
        { params::kHatLevel, kick.hatLevel },
        { params::kHatTone, kick.hatTone },
        { params::kHatDecay, kick.hatDecay },
        { params::kDrive, kick.drive },
        { params::kGateMix, kick.gateMix },
        { params::kReverseMode, static_cast<float>(kick.reverseMode) },
        { params::kReverseMix, kick.reverseMix },
        { params::kCrushMix, kick.crushMix }
    } };
    return preset;
}

} // namespace

const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> result;
        std::ranges::transform(kTranceKicks, std::back_inserter(result), makeFactoryPreset);
        return result;
    }();
    return presets;
}

const std::vector<FactoryPreset>& fillBuildPresets() {
    static const std::array<float, core::kNumFillPresets> tailShorten{
        0.54f, 0.7f, 0.82f, 0.64f, 0.88f, 0.72f, 0.48f, 0.66f, 0.42f, 0.76f,
        0.84f, 0.9f, 0.78f, 0.62f, 0.58f, 0.74f, 0.8f, 0.86f, 0.68f, 0.9f,
        0.72f, 0.82f, 0.64f, 0.88f, 0.76f, 0.92f, 0.7f, 0.86f
    };
    static const std::array<float, core::kNumFillPresets> pitchRise{
        0.0f, 4.0f, 7.0f, 2.0f, 10.0f, 6.0f, 3.0f, 5.0f, 0.0f, 8.0f,
        9.0f, 12.0f, 6.0f, 4.0f, 1.0f, 5.0f, 12.0f, 10.0f, 7.0f, 11.0f,
        8.0f, 12.0f, 3.0f, 9.0f, 6.0f, 12.0f, 4.0f, 10.0f
    };
    static const std::vector<FactoryPreset> presets = [] {
        std::vector<FactoryPreset> result;
        std::ranges::for_each(std::views::iota(0, core::kNumFillPresets), [&](int index) {
            result.push_back(FactoryPreset{
                core::kFillPresetNames[static_cast<size_t>(index)].data(),
                {
                    { params::kAutoFillPreset, static_cast<float>(index) },
                    { params::kAutoGrid, 4.0f },
                    { params::kAutoTailShorten, tailShorten[static_cast<size_t>(index)] },
                    { params::kAutoPitchRise, pitchRise[static_cast<size_t>(index)] }
                }
            });
        });
        return result;
    }();
    return presets;
}

void apply(
    const FactoryPreset& preset,
    juce::AudioProcessorValueTreeState& state,
    uint16_t sectionLockMask) {
    std::ranges::for_each(state.processor.getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr || isLockedParameter(sectionLockMask, ranged->paramID)) {
            return;
        }
        ranged->setValueNotifyingHost(ranged->getDefaultValue());
    });
    std::ranges::for_each(preset.values, [&](const auto& value) {
        if (isLockedParameter(sectionLockMask, value.first)) {
            return;
        }
        if (auto* parameter = state.getParameter(value.first)) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value.second));
        }
    });
}

void applyToCurrentAutoSlot(
    const FactoryPreset& preset,
    juce::AudioProcessorValueTreeState& state,
    uint16_t sectionLockMask) {
    if (sectionLocked(sectionLockMask, params::Section::autoMode)) {
        return;
    }
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
