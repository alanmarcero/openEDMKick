# openKicker v3 Design

**Date:** 2026-11-13
**Status:** Implementation direction derived from the user's request.

## Intent

Make openKicker produce finished, forceful trance kicks with a bright, physical
attack and a tuned low body. Every external MIDI note sets the kick fundamental
directly: MIDI 69 is A4 at 440 Hz and MIDI 57 is A3 at 220 Hz. Remove the UI
controls and freeze/lock paths that can override that pitch.

## Approaches

1. **Add harmonics to the v2 sine voice.** Small and low risk, but keeps the
   sine-led sound and control model the user asked to replace.
2. **Use pitched kick samples.** Can deliver a familiar recorded attack, but
   requires a sample bank and pitch shifting that can soften or color the
   transient across octaves.
3. **Build a MIDI-centered oscillator voice.** Use an oversampled body with a
   fast pitch dive that lands exactly on the incoming MIDI frequency, a
   resonant body knock, controllable harmonic saturation, and a short beater/noise
   transient. This is the selected direction: it provides authored, repeatable
   sounds over the MIDI range without an external sample dependency.

## Goals

- Make the MIDI note number the only root-pitch source. Remove root note,
  octave, fine-tune, pitch-lock, WAV-freeze, and MIDI fixed-pitch selection from
  the user interface and active synthesis path.
- Replace the sine-only voice with a more impactful kick voice. Keep the
  end-of-sweep pitch equal to the MIDI note's standard equal-tempered frequency.
- Redesign the native editor around the kick's body, transient, and tail, with
  a read-only current MIDI note/frequency display and no pitch picker.
- Replace every factory kick preset and every Auto Mode fill preset with new
  v3 material shaped for the new engine.
- Keep sequencing, effects, host transport, preset navigation, and MIDI one-shot
  behavior where they remain compatible with direct MIDI pitch tracking.
- Publish as v3.0.0 while keeping the existing JUCE plugin identity and format
  targets.

## Non-goals

- Do not change the product name, bundle ID, manufacturer/plugin codes, JUCE
  version, or supported plugin formats.
- Do not add sample libraries or third-party dependencies.
- Do not run or add tests in this task; compile the plugin and inspect the
  changed source paths instead.

## Sound and pitch behavior

`noteFrequencyHz(int midiNote)` uses `440 * 2^((midiNote - 69) / 12)` with no
root, octave, or cent offsets. The oscillator sweep starts above the note and
lands at that exact frequency. Any Auto Mode trigger without an incoming note
uses MIDI note 36 (C2); external MIDI note-ons always use their own note number.
Pattern editing controls rhythm and velocity, not pitch.

The body uses a phase-reset oscillator at four-times host rate when available,
an adjustable fast sweep, a resonant knock, upper partials controlled by body
tone, and a selectable saturation curve. A separate beater/noise transient
provides the first-millisecond edge. A bounded one-shot envelope shapes attack,
hold, and decay. All presets set the complete new synth state; presets cannot
change pitch because pitch is supplied at trigger time.

## UI direction

Use a compact instrument-console layout rather than the previous chromatic
keyboard and section-lock grid. The Kick page leads with the preset name, a
large waveform/pitch trace, and four left-aligned control groups: **Dive**,
**Body**, **Transient**, and **Tail**. A read-only note/frequency readout follows
the last MIDI hit. Pattern and FX pages retain their existing roles but use the
same new visual system. There is no note selector, octave/fine control, fixed
MIDI mode, pitch lock, Lock In WAV button, or piano-roll pitch row.

Visual tokens:

- Deep aubergine `#171521`, graphite `#292736`, signal violet `#A98BFF`,
  hot coral `#FF705E`, bright brass `#E9B94D`, and pale warm gray `#E9E6EF`.
- Use JUCE's cross-platform sans face for controls and a compact monospaced face
  for the MIDI note, Hz, and timing readouts.
- Keep labels sentence case, section baselines aligned, and color tied to sound:
  violet for body/pitch, coral for transient, brass for tail/level.
- Keep animation limited to the waveform reacting to sound and the current MIDI
  readout updating after a hit.

## Compatibility and constraints

- Keep C++20, JUCE 8.0.9, plugin identity, MIDI input/output declarations,
  build formats, and CMake target names.
- Keep Auto Mode, reverse sync, effects, output meter, preset navigation, and
  host transport behavior; Auto Mode uses C2 as its fixed trigger note.
- Remove obsolete pitch-selection and WAV-freeze parameters from the active
  parameter layout. Existing state values for removed controls are ignored.
- Do not add dependencies. A build with `-DOEK_BUILD_TESTS=OFF` is the local
  compile check; CI remains responsible for its test suite.

## Success checks

- The plugin builds for the existing targets without tests enabled.
- `noteFrequencyHz(69)` resolves to 440 Hz and `noteFrequencyHz(57)` to 220 Hz;
  the voice receives MIDI note numbers unchanged.
- No kick-page pitch selector, octave/fine control, lock control, or WAV-freeze
  control remains.
- All factory kick and Auto Mode preset names/data are replaced and unique.
- The README describes v3 and contains no broken links to removed v2 screenshots.
