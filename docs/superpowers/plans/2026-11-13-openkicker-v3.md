# openKicker v3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use $executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the sine-led kick voice and pitch-selection UI with a more forceful trance synth that tracks incoming MIDI notes exactly, then release the new instrument as v3.

**Architecture:** Keep the JUCE plugin shell, identity, formats, Auto Mode scheduler, and effects. Replace the synth voice and its active parameter model; have each MIDI trigger pass its note unchanged. Rebuild the Kick page, retain Pattern/FX functions, and author complete new kick and fill banks.

**Tech Stack:** C++20, JUCE 8.0.9, CMake. No new dependencies. Do not add or run tests; compile with tests disabled.

**Spec:** `docs/superpowers/specs/2026-11-13-openkicker-v3-design.md`

## Global Constraints

- MIDI 69 is A4 at 440 Hz and MIDI 57 is A3 at 220 Hz; no root, octave, or fine-tune offset changes that frequency.
- Auto Mode uses MIDI 36 (C2) and pattern editing controls rhythm and velocity only.
- Remove root-note, octave/fine-tune, fixed-pitch mode, section pitch locks, and WAV-freeze controls from the active instrument path.
- Keep the plugin identity, JUCE 8.0.9, formats, CMake targets, effects, sequencing, and host transport.
- Release version and tag are v3.0.0. Commit and push `main` with the personal Git identity from the repo instructions.
- Do not add or run tests.

## Review Focus

- Boundary MIDI notes and non-finite synth parameters must not cause invalid oscillator frequency or non-finite output; review note clamping and parameter sanitization in the voice.
- Auto Mode and incoming MIDI can trigger on the same sample; preserve event ordering and use C2 only for Auto Mode events.
- Preset loading must set every new synth parameter so the result does not depend on the previously selected preset.
- Removed pitch controls must not remain attached to an active parameter or UI path.
- The Pattern page must not expose note pitch while retaining step and velocity editing.

---

## File map

- `Source/Core/KickSynth.h/.cpp`: direct MIDI frequency, swept harmonic body, transient, bounded one-shot envelope, oversampling, and offline rendering helper.
- `Source/Core/KickEngine.h/.cpp`: pass note numbers directly to `KickVoice`; give Auto Mode C2; preserve effects and transport.
- `Source/Core/AutoSequencer.h/.cpp`: keep event timing and velocity, remove audible pitch editing, and replace the 28 factory fill names/content.
- `Source/Plugin/Parameters.h/.cpp`: expose the new sound controls plus existing sequence/effect controls; remove active root-pitch and WAV-freeze controls.
- `Source/Plugin/Presets.h/.cpp`: replace all 48 kick presets with complete v3 settings.
- `Source/Plugin/PluginProcessor.h/.cpp`: map new parameters to `KickSettings`, report the last MIDI note, and remove obsolete cache/lock state paths.
- `Source/Plugin/Theme.h`, `EdmLookAndFeel.*`, `Controls.*`, `Panels.*`, `PluginEditor.*`: rebuild the editor using Dive, Body, Transient, and Tail groups; keep Pattern and FX pages; remove pitch selection, lock buttons, and WAV freeze.
- `Tests/KickSynthTests.cpp`, `Tests/KickEngineTests.cpp`, `Tests/AutoSequencerTests.cpp`, `Tests/PluginTests.cpp`: update existing checks for direct MIDI and remove obsolete WAV-lock/pitch-picker expectations; add no tests and do not run them.
- `CMakeLists.txt`, `README.md`, `docs/AUDIO.md`: set v3.0.0 and document the new sound and MIDI behavior without stale v2 links.

## Tasks

### Task 1: Implement the direct-MIDI kick voice

**Files:** `Source/Core/KickSynth.h`, `Source/Core/KickSynth.cpp`, `Source/Core/KickDsp.h`, `Source/Core/KickDsp.cpp`

- [x] Keep `noteFrequencyHz(int midiNote)` free of tuning offsets and clamp input only to the supported MIDI range 0–127.
- [x] Implement a phase-reset swept oscillator whose final frequency is exactly the incoming note frequency, with body partials, resonant knock, saturation, beater/noise transient, and bounded attack/hold/decay envelope.
- [x] Keep rendering allocation-free per sample and use the existing oversampling/decimator helpers.
- [x] Remove WAV cache, pitch-root, and freeze code from the new voice interface.
- [x] Review sample-rate, tail-length, retrigger, and note-range handling in source.
- [x] Update existing synth checks for the new API and remove stale lock-cache expectations; add no tests and do not run them.

### Task 2: Connect MIDI and preserve Auto Mode/effects

**Files:** `Source/Core/KickEngine.h/.cpp`, `Source/Core/AutoSequencer.h/.cpp`

- [x] Change each external note-on path to call the voice with the MIDI note number unchanged.
- [x] Use MIDI 36 for Auto Mode and audition events; ignore former piano-roll pitch offsets.
- [x] Retain event timing, velocity, reverse cues, host/internal transport, effect ordering, stop/seek behavior, and limiter.
- [x] Replace all 28 Auto Mode factory names and pattern data; retain the existing bar/grid ranges and slot editing.
- [x] Update existing sequencer/engine checks to match the pattern model; add no tests and do not run them.

### Task 3: Replace synth parameters and author presets

**Files:** `Source/Plugin/Parameters.h/.cpp`, `Source/Plugin/Presets.h/.cpp`, `Source/Plugin/PluginProcessor.h/.cpp`

- [x] Define parameters for sweep octaves/time/shape, snap octaves/time, body drive/type/knock/tone/grit/sub mix/phase, transient type/level/frequency/duration, attack/hold/decay/shape, low cut, and output clip.
- [x] Remove obsolete tuning, fixed MIDI mode, Lock In WAV, section-lock, and frozen-WAV state from the active processor path.
- [x] Keep sequence and effect parameters stable and map the new synth parameters to `KickSettings`.
- [x] Track the latest external MIDI note for a read-only display; initialize the display and Auto Mode note to C2.
- [x] Replace all 48 sound presets with unique names and complete new settings; loading any preset sets every synth field.
- [x] Update existing processor expectations to remove frozen-WAV and pitch-picker behavior; add no tests and do not run them.

### Task 4: Redesign the native editor

**Files:** `Source/Plugin/Theme.h`, `Source/Plugin/EdmLookAndFeel.h/.cpp`, `Source/Plugin/Controls.h/.cpp`, `Source/Plugin/Panels.h/.cpp`, `Source/Plugin/PluginEditor.h/.cpp`

- [x] Use the spec palette and aligned four-group Kick page: Dive, Body, Transient, Tail.
- [x] Keep preset navigation, waveform/pitch trace, read-only MIDI note/frequency, Simple/Advanced control density, Pattern page, FX page, meter, and bypass.
- [x] Remove note buttons, octave/fine controls, fixed MIDI mode, section locks, Lock In WAV, and piano-roll pitch rows.
- [x] Keep piano-roll step/velocity editing and show every pattern trigger on one drum lane.
- [x] Check all component IDs and parameter attachments after removing old controls.

### Task 5: Update docs, version, and release

**Files:** `CMakeLists.txt`, `README.md`, `docs/AUDIO.md`, `docs/SCREENSHOTS.md`

- [x] Set CMake/plugin version to 3.0.0 and describe v3 without links to deleted v2 images.
- [x] Document exact MIDI frequency behavior, new synthesis sections, Auto Mode's C2 note, and retained effects/transport.
- [x] Configure with tests disabled and build all JUCE formats; do not run tests.
- [x] Review the source mapping and preset completeness, run `git diff --check`, and compare `git diff --stat` with `git diff -w --stat`.
- [x] Commit with `git -c user.name="Alan Marcero" -c user.email="221119764+alanmarcero@users.noreply.github.com" commit ...`.
- [x] Before pushing, run `git log --format='%h %ae %ce' origin/main..HEAD` and confirm every email uses the noreply address. Push `main` and tag `v3.0.0` to the configured origin.
