# openKicker v2 Design

**Date:** 2026-11-12
**Status:** Builder check complete; the user's "entire plugin" instruction
includes the synthesis, sequencing, effects, engine, processor, and editor
implementations.

## Intent

Rebuild openKicker as a more forceful EDM and trance kick instrument. The present
kick body is mostly a sine oscillator, so it often reads as a deep sub rather than
a finished club kick. V2 will give the body a fuller harmonic attack and a complete
one-shot ADSR while keeping the instrument's tuning, sequencing, MIDI, effects,
layering, cached-WAV, and performance features.

The editor will be redrawn around sound-shaping sections. Each section gets a
lock control so browsing a preset can leave selected settings intact. Both preset
banks are replaced: 48 new trance kick programs and 28 new trance Auto Mode
patterns. No factory presets from the prior implementation remain.

## Current feature inventory

The existing plugin is a JUCE C++20 instrument with a pure C++ DSP core. It
builds VST3, AU, LV2, and Standalone formats, with the existing macOS, Windows,
and Linux release workflow. The following user-visible features remain in V2:

- 12-note chromatic tuning, three octaves, fine cents, Fixed and MIDI Note
  trigger modes, velocity response, audition trigger, and waveform preview.
- Pitch start, pitch decay and curve; Tail Length, Tail Curve, Punch, Sub Drive,
  Click Level and Click Frequency; Lock In WAV with saved float WAV data and
  playback at a changed host sample rate.
- Snare, clap, and hat layers with their independent level, tone, and decay
  controls; clap spread remains the flam control.
- Simple and Advanced editor modes, preset previous/next navigation, program
  selection, bypass, peak meter, and the animated Kicking Mule stage.
- Auto Mode with independent A/B/C/D slots, a 1/64-capable piano roll, editable
  triggers and pitch offsets, 1/4 through 1/64 grids, 1 to 64 bar lengths,
  internal or host sync, tempo and meter, Tail Shortener, Pitch Rise, Crush Ramp,
  Auto Bypass, and audition during playback.
- Reverse Sync Off, Beat End, Beat 4 Auto, half-beat, and quarter-beat modes;
  Drive, Bitcrush, Beat Stutter, Sync Gate, Filter Pump, high-pass, low-pass,
  output gain, limiter, ceiling, and master bypass.
- Existing MIDI behavior: omni input; Fixed or Note mode; one-shot note-offs;
  velocity-zero note-ons do not trigger; CC 7, 11, 74, 120, 121, and 123 keep
  their current meanings; all other controllers remain ignored.
- The 28 Auto Mode pattern slots remain available, but their factory names and
  note data are all newly authored for trance builds and rolls.

## Goals

- Produce a kick with a clear transient and audible upper harmonics while
  retaining controlled, tuned low-frequency weight.
- Add independent Attack, Decay, Sustain, and Release controls. Tail Length
  remains the total one-shot duration; Release shapes the end of that duration.
- Add a Body Tone control that moves from a round fundamental toward a brighter,
  harmonic-rich kick. Keep Punch, Click Level/Frequency, and Sub Drive available.
- Parameter ranges: Body Tone 0–1 (default 0.65), Attack 0.05–20 ms (0.35 ms),
  Decay 1–600 ms (72 ms), Sustain 0–1 (0.70), and Release 5–800 ms (180 ms).
- Add section locks. Loading a preset must not change parameters that belong to
  a locked section. Locks do not disable manual editing.
- Replace the full preset catalog with exactly 48 newly authored trance kick
  presets and replace all 28 Auto Mode fill patterns with new trance patterns.
- Redesign the complete native editor and update the README, audio notes, and
  native screenshots.
- Preserve plugin identity, existing parameter IDs and versions, MIDI behavior,
  session recall, formats, and the old settings that already load from saved
  sessions.

## Non-goals

- Do not change the plugin codes, bundle ID, product name, format list, JUCE
  dependency, or host automation IDs already in use.
- Do not remove sound layers, Auto Mode capabilities, effects, MIDI mappings,
  preset navigation, the waveform, Simple/Advanced mode, or the Kicking Mule.
- Do not retain old factory kick names, values, indexes, or Auto Mode pattern
  names/data in the new preset banks.
- Do not add external sample dependencies or new third-party dependencies.

## Approaches

1. **Layer the current sine oscillator.** Smallest DSP change, but leaves the
   sound and editor architecture that prompted the request in place.
2. **Keep both old and new synth paths.** Reduces migration risk but adds a
   parallel engine, more UI choices, and permanent duplicate maintenance.
3. **Rebuild the synth and editor around the existing host contract.** Replace
   the synthesis, sequencer, effects, engine, processor, and editor
   implementations, but retain JUCE, the plugin identity, established parameter
   IDs, MIDI/transport rules, saved WAV support, and all existing feature
   contracts. This is the selected approach because the request calls for a
   complete reimplementation without feature loss.

## Sound and parameter behavior

The new kick voice combines a tuned body, a short harmonic beater component,
the existing click/noise character, Punch, and saturation. Body Tone controls
the harmonic mix. Each new kick preset sets the full kick-synthesis parameter
set so its sound does not depend on which preset was loaded before it.

The one-shot amplitude envelope uses four stages:

- Attack rises from silence to the transient peak.
- Decay falls from the peak to Sustain.
- Sustain holds until the configured release window begins.
- Release fades to silence at the end of Tail Length.

The envelope is bounded to Tail Length, including when stage settings exceed
the available time. Existing Tail Curve continues to shape the one-shot tail.
Layer-specific decays keep their current independent duration behavior, and
Auto Mode Tail Shortener still bounds the complete hit on rapid rolls.

The old 68-program catalog and old pattern names/content are retired. The new
factory catalog has 48 programs, indexed 0 through 47, with original trance
sound designs spanning uplifting, progressive, classic, psy, hard-trance,
tech-trance, and festival styles. The separate Auto Mode bank retains 28 slots,
all with new names and new patterns. Manual note editing, pitch offsets,
reverse cues, and all bar/grid options remain available.

Saved parameter state from earlier versions continues to load, including a
cached Lock In WAV. Old factory preset indexes are not mapped to old preset
content. A session without the V2 catalog marker is shown in the editor as a
restored custom/session sound; its saved parameter values remain active until a
new V2 preset is selected. Auto Mode preset indices resolve to the new pattern
bank because the prior patterns are retired.

## Section locks

Locks are editor/session state, saved with the plugin state but not exposed as
audio automation parameters. A lock protects every setting in its section from
factory preset loading; users can still edit those controls by hand.

The editor sections and preset parameter ownership are:

| Section | Settings protected during preset loading |
| --- | --- |
| Tuning | Note, octave, fine tune, MIDI trigger mode |
| Pitch | Start Pitch, Pitch Decay, Pitch Curve |
| Body & Envelope | Tail Length, Tail Curve, Body Tone, ADSR, Punch, Sub Drive, Lock In WAV |
| Transient | Click Level, Click Frequency |
| Layers | Snare, clap, and hat level, tone, spread, and decay |
| Auto Mode | Enable/bypass, slot, clock sync, tempo, meter, and all A/B/C/D settings |
| Reverse | Reverse mode and mix |
| Texture | Drive and all Bitcrush controls |
| Rhythm | Beat Stutter, Sync Gate, and Filter Pump settings |
| Tone & Output | High/low-pass, Output, Limiter, Ceiling, and Master Bypass |

Manual `setCurrentProgram` and UI preset selection use the same lock-aware
application path. Factory program loading keeps its existing behavior of
resetting unspecified, unlocked parameters to their APVTS defaults before
applying the selected program. It snapshots and restores every value in locked
sections across that reset. Quick selection of an Auto Mode pattern respects
the Auto Mode lock and otherwise changes only the active slot's pattern
settings.

## Editor design

The UI is a complete redraw with persistent top-level transport/preset controls
and three pages: **Kick**, **Pattern**, and **FX**. Each sound and effect group
is presented as an editable section with its own lock button. Simple mode shows
the primary controls and Advanced mode reveals every fine control; both modes
retain all sections and features.

Visual direction: a compact signal console inspired by trance rack gear and
oscilloscope traces, not a skeuomorphic recreation of the current faceplate.

- Palette: ink navy `#111824`, steel blue `#202B38`, ice signal `#4EC5E8`,
  transient coral `#FF715B`, tail amber `#E9B971`, and warm readout `#E9E4D8`.
- Type: JUCE's cross-platform sans family for labels and controls; monospaced
  numerals for pitch, milliseconds, tempo, and meter readouts.
- Alignment: left-aligned section titles, consistent control baselines, and a
  wide central waveform/roll rather than centered marketing-style text.
- Kick page: chromatic key strip, waveform and pitch trace, compact Kicking Mule
  animation, and clearly separated tuning, pitch, body/envelope, transient, and
  layer sections.
- Pattern page: A/B/C/D selectors and transport settings above the editable
  piano roll; Tail Shortener and build controls remain adjacent to the roll.
- FX page: readable reverse, texture, rhythm, tone, and output sections with a
  clear audio path and live peak meter.
- Lock buttons visibly distinguish locked and unlocked state and have tooltips.

## Constraints

- Stay on JUCE 8.0.9 and C++20; add no dependency.
- Preserve `com.alanmarcero.openedmkick`, manufacturer/plugin codes, APVTS root,
  existing parameter IDs and parameter versions.
- Keep the current release formats, CMake targets, and cross-platform package
  workflow.
- Publish as **v2.2.0**: the existing release is v2.1.0 and the major version
  remains 2 as requested.
- Commit to `main` using the personal Git identity required by the repo
  instructions. Push `main` and the `v2.2.0` tag so the release workflow can
  publish packages.
- Add actual native editor screenshots for Kick, Pattern, and FX pages to the
  repository and reference them in the README.

## Success checks

- The build produces the existing plugin formats with the same JUCE/plugin
  identities.
- Existing saved parameter trees and cached WAV data load; all listed feature
  contracts remain present.
- The 48 new kick presets and 28 new Auto Mode patterns have distinct names and
  authored settings/content; no old factory preset name or content remains.
- A preset change leaves every locked section parameter unchanged while
  applying the new preset's values to unlocked parameters.
- The editor exposes every section in both modes and shows each section lock.
- README and audio notes describe the new engine, preset bank, section locks,
  build/release, and screenshots.
- Three screenshots show the built native editor, one each for Kick, Pattern,
  and FX.
