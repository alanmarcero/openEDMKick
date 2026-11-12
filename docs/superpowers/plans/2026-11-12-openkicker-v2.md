# openKicker V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use $executing-plans to implement this plan task-by-task.

**Goal:** Rebuild openKicker's DSP, sequencing, effects, state integration, preset banks, and native editor for a more impactful trance kick while retaining every existing feature contract.

**Architecture:** Keep JUCE, the plugin identity, host-visible parameter IDs, and MIDI/state contracts. Replace the internal DSP and UI implementations; add a harmonic kick body, one-shot ADSR, lock-aware factory preset application, 48 new kick programs, 28 new Auto Mode patterns, and a three-page editor.

**Tech Stack:** C++20, JUCE 8.0.9, CMake, existing Catch2 test suite (updated assertions only; no tests are added or run in this session).

**Spec:** `docs/superpowers/specs/2026-11-12-openkicker-v2-design.md`

## Global Constraints

- Preserve plugin ID `com.alanmarcero.openedmkick`, manufacturer/plugin codes, APVTS root, all existing parameter IDs and versions, and existing format targets.
- Do not retain any of the previous 68 kick presets or 28 Auto Mode pattern names/content.
- Provide exactly 48 newly authored trance kick programs and 28 newly authored trance Auto Mode patterns.
- Keep manual editing, host/internal sync, MIDI behavior, layer controls, WAV lock/cache, reverse sync, FX, meter, mascot stage, and Simple/Advanced modes.
- Add no dependencies and use the existing JUCE/CMake conventions.
- Preserve the existing editor's 1100 × 972 default size so hosts retain familiar window bounds.
- Do not add or run tests. Update existing preset-name/count expectations where the new catalogs make them stale; use source review and a successful plugin build as local checks.
- Release as v2.2.0 on `main`, committing with Alan Marcero's personal Git identity and pushing both `main` and the `v2.2.0` tag.

## Review Focus

- A locked section must retain every owned parameter when a factory program loads; manually editing that section must still work. Review the parameter ownership table against the preset-application filter.
- A pre-V2 state must restore its parameter values and cached WAV while the UI identifies the old catalog selection as a restored session sound.
- New ADSR values must remain bounded by Tail Length and end at silence; rapid Auto Mode triggers must retain Tail Shortener behavior.
- Each new kick preset must set all kick-synthesis fields; each Auto Mode preset must generate non-empty note data within its selected bar count.
- MIDI, transport, reverse boundaries, FX ordering, limiter, and output trim must preserve their current documented behavior.

---

## File map

- `Source/Core/KickSynth.h/.cpp`: kick settings, harmonic oscillator, transient, one-shot ADSR, deterministic layers, WAV cache, and live/reverse voices.
- `Source/Core/AutoSequencer.h/.cpp`: unchanged public scheduling surface with the new 28 pattern names and authored note data.
- `Source/Core/Effects.h/.cpp`: host-synced reverse calculations and the drive, crusher, stutter, gate, pump, filters, and limiter implementation.
- `Source/Core/KickEngine.h/.cpp`: transport/MIDI/Auto Mode orchestration and effect order.
- `Source/Plugin/Parameters.h/.cpp`: stable legacy parameters plus Body Tone and ADSR parameters.
- `Source/Plugin/Presets.h/.cpp`: 48 new complete kick programs and lock-aware application.
- `Source/Plugin/PluginProcessor.h/.cpp`: JUCE bridge, old-session restore, lock-state storage, and 48-program catalog.
- `Source/Plugin/Theme.h`, `EdmLookAndFeel.h/.cpp`, `Controls.h/.cpp`, `Panels.h/.cpp`, `PluginEditor.h/.cpp`: complete three-page UI, per-section locks, visual refresh, and retained features.
- `Tests/PluginTests.cpp`, `Tests/AutoSequencerTests.cpp`: update existing catalog name/count expectations only.
- `CMakeLists.txt`, `README.md`, `docs/AUDIO.md`, `docs/SCREENSHOTS.md`, `assets/openKicker-v2-*.png`: version, release description, sound notes, and native screenshots.

## Tasks

### Task 1: Rebuild the kick voice and ADSR

**Files:**
- Modify: `Source/Core/KickSynth.h`
- Modify: `Source/Core/KickSynth.cpp`

**Interfaces:**
- Preserve `fundamentalHzForNote`, `shapeCurve`, `pitchSweepGain`,
  `renderKickWaveform`, WAV encode/decode, `KickWavCache`, and `KickVoice` public
  call signatures.
- Add `bodyTone`, `attackMs`, `decayMs`, `sustainLevel`, and `releaseMs` to
  `KickSynthSettings`; defaults produce a punch-forward, harmonically present F1
  kick while retaining the tuned tail.
- Keep Tail Length as the complete one-shot length. Attack rises to peak, Decay
  reaches Sustain, Sustain holds until the Release window, and Release reaches
  silence at Tail Length. Clamp stage timing safely when stages exceed the hit.
- Keep layer-specific decay and spread controls independent from the new body
  envelope. Keep deterministic noise, WAV sample-rate conversion, retrigger
  de-click, and reverse voice playback.

**Check:** `cmake --build build --config Release --parallel` must compile the core
and plugin after dependent tasks are complete. Do not execute tests.

### Task 2: Rebuild Auto Mode patterns and scheduling

**Files:**
- Modify: `Source/Core/AutoSequencer.h`
- Modify: `Source/Core/AutoSequencer.cpp`

**Interfaces:**
- Preserve `kNumAutoSlots == 4`, `kNumFillPresets == 28`, bar/grid constants,
  `PianoRollNote`, `AutoSlotSettings`, `AutoTriggerEvent`, and all `AutoSequencer`
  method signatures.
- Replace every old fill name and pattern. Author 28 distinct trance patterns
  across four-on-floor, progressive climb, uplifting roll, psy triplets,
  syncopation, reverse cues, and 1/16 through 1/64 finales.
- Preserve note editing, pitch offsets, velocity, reverse cues, selected grid,
  bars, host PPQ/internal clock behavior, and Auto Mode Tail Shortener.

**Check:** Review every pattern generator branch for a valid non-empty pattern
and bar-range clamp. Existing AutoSequencer tests remain the CI regression gate;
do not run them locally.

### Task 3: Rebuild effects and engine orchestration

**Files:**
- Modify: `Source/Core/Effects.h/.cpp`
- Modify: `Source/Core/KickEngine.h/.cpp`

**Interfaces:**
- Preserve `ReverseSyncMode`, all sync divisions, public reverse timing helpers,
  `BitCrusher`, `BeatStutter`, `TempoGate`, `SyncFilterPump`, `ToneFilter`,
  `PeakLimiter`, `Transport`, MIDI event structs, `EffectSettings`,
  `EngineSettings`, and `KickEngine` public methods.
- Reimplement voice scheduling, reverse cancellation on host stop/seek, MIDI CC
  behavior, Auto Mode host/internal timing, and the effect chain while keeping
  zero-latency limiting, output routing, smoothing, and finite output.
- Preserve the established processing order: source/layers, drive/crush/rhythm,
  tone filters, output gain, and limiter; keep reverse mixing and brightness CC.

**Check:** Compare method behavior with `docs/AUDIO.md` feature contracts and
existing unit-test names. Do not execute tests.

### Task 4: Add parameters, preset banks, and session-aware locks

**Files:**
- Modify: `Source/Plugin/Parameters.h/.cpp`
- Modify: `Source/Plugin/Presets.h/.cpp`
- Modify: `Source/Plugin/PluginProcessor.h/.cpp`

**Interfaces:**
- Keep every existing APVTS parameter ID and version. Add `body_tone`,
  `amp_attack`, `amp_decay`, `amp_sustain`, and `amp_release` with documented
  labels, ranges, formatters, and defaults: 0–1/.65, 0.05–20 ms/.35 ms,
  1–600 ms/72 ms, 0–1/.70, and 5–800 ms/180 ms.
- Replace the current factory kick bank with exactly 48 new trance presets.
  Every preset has a unique new name and explicitly sets every kick synthesis
  field, including tuning, pitch, Body Tone, ADSR, Tail, Punch, Sub Drive,
  Click, and drum layer values. The 28 Auto Mode presets are supplied by Task 2.
- Do not retain old factory names, values, or patterns. Give the new catalog a
  V2 marker. Restore old parameter values and embedded Lock In WAV data; old
  catalog states display as a restored session/custom sound.
- Add ten non-automatable section lock flags held as UI/session state and saved
  in the plugin state tree. Locks map exactly to the table in the spec. Missing
  lock data in old sessions defaults to unlocked.
- Use one shared lock-aware preset application path for UI selection and host
  program changes. Snapshot locked values across the current reset-to-default
  factory behavior, then overlay the selected preset on unlocked parameters.
  Quick Auto Mode pattern selection respects the Auto Mode lock and otherwise
  changes only the active slot's pattern settings. All 48 new sound presets set
  their complete synthesis state.
- Update `currentSynthSettings`, APVTS parameter pointers, `getNumPrograms`,
  program name handling, and state restore for the new controls and catalog.

**Check:** Inspect each new preset's synthesis field list for completeness,
verify all 48 names are unique, verify lock ownership covers every presettable
parameter exactly once, and confirm legacy parameter IDs are unchanged.

### Task 5: Reimplement the native editor

**Files:**
- Modify: `Source/Plugin/Theme.h`
- Modify: `Source/Plugin/EdmLookAndFeel.h/.cpp`
- Modify: `Source/Plugin/Controls.h/.cpp`
- Modify: `Source/Plugin/Panels.h/.cpp`
- Modify: `Source/Plugin/PluginEditor.h/.cpp`

**Interfaces:**
- Keep the 1100 × 972 editor bounds and host-facing `OpenEdmKickEditor`
  methods, including `setAdvancedView` and `isAdvancedView`.
- Replace the old faceplate layout with persistent preset/transport controls and
  Kick, Pattern, and FX pages using the spec palette, alignment, and section map.
- Each of the ten editable sections has a lock button with visible state,
  accessible text/tooltips, and direct session-state attachment.
- Keep previous/next preset, preset selector, Simple/Advanced mode, Push It,
  Lock In WAV, 12-note keyboard, waveform/pitch trace, all audio controls,
  editable piano roll, A/B/C/D selectors, live meter, bypass, and animated
  Kicking Mule stage.
- Ensure both UI modes expose every section: Simple presents primary controls;
  Advanced reveals all detailed controls.

**Check:** Build the Standalone target and inspect the native editor at its
default size. Do not run screenshot tests.

### Task 6: Update release docs, screenshot assets, and stale expectations

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `docs/AUDIO.md`
- Modify: `docs/SCREENSHOTS.md`
- Modify: existing catalog assertions in `Tests/PluginTests.cpp` and
  `Tests/AutoSequencerTests.cpp`
- Add: `assets/openKicker-v2-kick.png`
- Add: `assets/openKicker-v2-pattern.png`
- Add: `assets/openKicker-v2-fx.png`

**Details:**
- Set project version to 2.2.0.
- Replace stale README content with the V2 sound engine, ADSR, lock sections,
  48-program/28-pattern bank, retained MIDI/features, build instructions, and
  screenshot links.
- Replace old audio notes with the one-shot ADSR law, harmonic body/transient
  model, lock behavior, and preserved MIDI/Auto Mode/effect contracts.
- Capture the compiled native editor's Kick, Pattern, and FX pages, and document
  the actual visible settings in `docs/SCREENSHOTS.md`.
- Adjust only existing test assertions that expect old preset names/counts;
  do not add test cases or run any tests.

**Check:** Inspect README image links and screenshot files; `git diff --check`;
review changed names to confirm no old preset names remain in the factory banks.

### Task 7: Build, review, version, and publish

**Files:** all files above.

**Steps:**

- [ ] Build plugin and Standalone: `cmake --build build --config Release --parallel`.
- [ ] Launch the built Standalone editor and capture the three native pages.
- [ ] Review the source diff for preserved feature contracts, complete lock
  ownership, preset counts/names, plugin IDs, and no unrelated changes.
- [ ] Run `git diff --check` and compare `git diff --stat` with
  `git diff -w --stat`; remove unrelated or whitespace-only churn.
- [ ] Update `CMakeLists.txt` and documentation to v2.2.0; create one release
  commit using the personal identity from the repo instructions.
- [ ] Before pushing, run
  `git log --format='%h %ae %ce' origin/main..HEAD` and confirm each email is
  `221119764+alanmarcero@users.noreply.github.com`.
- [ ] Push `main` and annotated tag `v2.2.0`; confirm the source commit and tag
  resolve to the same commit. GitHub Actions then builds, runs CI tests, and
  creates the release.
