# openEDMKick

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

A dedicated EDM kick drum synthesizer VSTi featuring 12-note chromatic sub tuning, one-click **Lock In** internal WAV caching, host-clock-synced kick effects (including Beat-4-aligned Reverse swells and Bitcrush), and an **A/B/C/D Auto Mode** with a **1/64-note Piano Roll** and an Auto-Mode-exclusive **Tail Shortener** knob to keep rapid build-up rolls punchy and mud-free. Release packages contain VST3, LV2 and a standalone app on macOS, Windows and Linux; macOS also includes AU.

![openEDMKick hero](assets/openEDMKick-hero.png)

## Plugin UI Screenshots

### 1/64-Note Drop Build & Locked-In WAV Cache (`Auto Mode A`)

![openEDMKick native editor - 1/64 Drop Rush & Locked-In WAV](assets/native-ui.png)

### Beat-4 Host-Synced Reverse Pickup Loop (`Auto Mode B`)

![openEDMKick native editor - Beat 4 Reverse Pickup Loop](assets/native-ui-reverse.png)

*Captured from the native v1.2.0 editor. [Capture details and provenance](docs/SCREENSHOTS.md).*

## Features

- **Dedicated EDM Kick Drum VSTi (`IS_SYNTH`)**. Built exclusively for crafting phase-accurate, club-ready kick drums. In Regular Mode, any incoming MIDI note triggers the kick drum at its tuned fundamental pitch.
- **Optional Snare, Clap, and Hat layers**. Three extra timbres blend onto the same hit. Each has a level, defaulting to off, plus one or two modifiers: snare tone and decay, clap flam spread and tone, hat tone and open/closed decay. They follow velocity, semitone offset, Lock In, and the waveform preview. Tail Length still ends the voice. See [docs/AUDIO.md](docs/AUDIO.md).
- **12-Note Chromatic Sub Tuning (`C` through `B`)**. Tune the kick fundamental directly to any of the 12 musical keys (`C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`) from a piano keyboard across three octave ranges (`0 Sub`, `1 Club`, `2 Punch`) with fine cent adjustment (`-50 .. +50 ct`), an exponential pitch drop (`Start Hz`, `Pitch Decay`, `Pitch Curve`), body/tail shaping (`Tail Length`, `Tail Curve`, `Sub Drive`, `Punch`), and a beater click (`Click Level`, `Click Freq`). The gold trace on the waveform is that pitch drop.
- **One-Click "Lock In" Internal WAV Cache (`LOCK IN (WAV)`)**. Freezes the synthesized kick into an internal 32-bit IEEE float RIFF `.wav` buffer managed entirely by the plugin. While locked in, the plugin stops synthesizing oscillators/envelopes in real time and plays directly from the cached WAV, guaranteeing 100% identical phase and transient impact and persisting the locked WAV inside the DAW session state. If the host sample rate changes, that same WAV is resampled instead of being synthesized again.
- **Host-Synced Kick Effects & Beat-4 Reverse Alignment**:
  - **Reverse Sync (`Off`, `Beat End (Sync)`, `Beat 4 Auto`, `1/2 Beat`, `1/4 Beat`)**: Locks reverse kick swells to the host transport. When triggered on Beat 4 of a bar, the reverse kick automatically calculates the remaining samples in Beat 4 and finishes at the exact end of Beat 4.
  - **Bitcrusher & Sample-Rate Reducer**: `Rate` (`200 Hz – 48 kHz`), `Bits` (`1 – 16`), and `Mix`, plus automatic build-up ramp modulation in Auto Mode.
  - **Beat Stutter (`1/4` to `1/64`)**: Host-synced beat repeat that slices and re-triggers the kick transient on the beat grid.
  - **Sync Gate & Filter Pump (`1/4` to `1/64`)**: Host-clock-synced rhythmic gate chopping and resonant filter pumping.
  - **Drive & Zero-Latency Peak Limiter**: level-compensated `tanh` saturation and a ceiling that catches the sample which crosses it. Reported plugin latency stays at 0, so the transient stays on the MIDI grid.
- **A/B/C/D Auto Mode & 1/64-Note Piano Roll**:
  - **Four Automatable Slots (`A`, `B`, `C`, `D`)**: Switch between four independent kick build-up and fill patterns from the UI or the host's `Auto Mode` automation lane, just like `openEDMFilter`.
  - **1, 2, 4, 8, 16, 32, and 64 Bar Lengths**: Loop patterns across any phrase window from single-bar turnarounds to 64-bar marathon risers.
  - **12 EDM Kick Fill & Build-Up Presets**: Includes `Four on the Floor + End Fill`, `Classic EDM Build (1/4 -> 1/32)`, `Festival 1/64 Drop Rush`, `Tech House Groove & Turnaround`, `Big Room Drop Countdown`, `Hardstyle Pitch-Up Build`, `Future Rave Syncopated Fill`, `Halftime to 1/32 Blaster`, `Beat 4 Reverse Pickup Loop`, `Accelerating Exponential Roll`, `Offbeat Stomp & 1/64 Finale`, and `Mainstage 64th Machine Gun`.
  - **Interactive 1/64-Note Piano Roll**: View and edit kick triggers and semitone pitch offsets (`ROOT` to `+12 st`) down to `1/64` note resolution.
  - **Auto-Mode-Specific Tail Shortener (`Tail Shorten`)**: Automatically shortens the kick tail during Auto Mode (especially on fast `1/16`, `1/32`, and `1/64` rolls) so rapid build-ups never get muddy. This control is exclusive to Auto Mode and never alters the kick tail in Regular Mode.
- **Kicking Mule Stage**: Features the 3D **Kick Mule** mascot kicking in front of a pulsing studio subwoofer cone and stage spotlights synced to the kick output and host beat phase.

## Engine notes

Parameter IDs, ranges, and saved sessions from v1.0.0 and v1.1.0 still load. New layer parameters are added with level 0, so an older session stays a dry kick until those knobs are raised. The v1.1 oscillator is unchanged when the layers are off. A project that was not Locked In will not sound sample-identical to v1.0.0. A frozen Lock In WAV is restored as-is.

- The pitch drop is exponential and reaches the tuned note at the end of Pitch Decay. A positive Pitch Curve still falls faster than a negative one.
- Tail Curve keeps that polarity and is exponential, so the body does not ramp down in a straight line.
- The click is a short chirp plus a deterministic noise tick. Live playback matches Lock In, including the click.
- Retriggering fades the previous sample out in about 0.45 ms.
- With Host Sync on, Auto Mode runs only while the host transport is playing. TRIG still auditions the tuned kick. With Host Sync off, the internal clock keeps running.
- Objective checks for the default F1 club kick at 48 kHz: settled pitch 43.67 Hz against the 43.65 Hz fundamental, DC about `-7e-6`, and peak `-0.89` with no full-scale samples. See [docs/AUDIO.md](docs/AUDIO.md).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Layout

- `Source/Core`: Pure C++20 DSP with no JUCE dependency (`KickSynth`, `AutoSequencer`, `Effects`, `KickEngine`).
- `Source/Plugin`: JUCE processor, parameters, presets, custom look-and-feel, piano roll, and editor panels.
- `Tests`: Deterministic Catch2 unit and plugin tests.
