# V2 audio engine

openKicker V2 replaces the kick voice, pattern clock, effect implementations, and output path. The engine remains a self-contained C++20 library with no dependency on JUCE.

## Kick voice

The body uses a tunable oscillator whose pitch falls from Start Pitch to the selected note. Body Tone adds a decaying set of upper harmonics to the fundamental, and Body Drive adds level-compensated saturation. Punch adds a fast-decaying attack boost. The transient layer combines a chirped tone with deterministic noise so the first few milliseconds read clearly on a dance mix.

The amplitude envelope has Attack, Decay, Sustain, and Release controls. It is a one-shot envelope: no MIDI note-off is required. Tail Length sets the maximum body duration, Tail Curve sets the body falloff, and Release is fit inside the tail. The waveform view previews the current synthesized hit.

Snare, clap, and metallic-hat layers remain optional. Their levels default to zero. Each layer has its own tone and decay controls; the clap also has spread. Layers follow velocity and piano-roll pitch. A frozen WAV captures the full current kick and layer blend.

## Pattern clock

The sequencer has four independent slots. Each stores a fill selection, bar count, grid, Tail Shortener, pitch rise, and crush ramp. The 28 factory fills have new names and note content. Their generated rolls use a four-on-the-floor anchor, varied syncopations and subdivisions, pitch accents, and optional reverse cues. Density and accents change across the selected phrase.

The piano roll stores trigger position, gate length, velocity, pitch offset, and reverse cue. Host sync uses the host PPQ and tempo; internal clock uses the selected tempo and meter. Auto Mode does not replace Regular Mode MIDI triggering. A separate audition trigger works in either mode.

The Tail Shortener estimates time to the next pattern hit and reduces the current kick's tail for fast rolls. It only applies to Auto Mode triggers. MIDI hits in Regular Mode keep the full Tail Length.

## Effect path

The reverse voice is triggered against the selected musical boundary and is cut on transport stop, seek, or loop discontinuity. Forward and reverse voices can be blended. The remaining signal path is:

1. Drive saturation.
2. Sample-rate and bit-depth reduction, blended with the dry signal.
3. Host-synced beat repeat, blended with the dry signal.
4. Rhythmic low-pass pump.
5. Tempo gate.
6. Output gain and MIDI Volume/Expression.
7. Smoothed high-pass and low-pass filters, plus MIDI Brightness ceiling.
8. Optional zero-latency peak limiter.

The high-pass and low-pass each use two cascaded one-pole stages. The effect returns the input at the 20 Hz high-pass and 20 kHz low-pass bypass points. The limiter applies gain reduction on the sample that crosses the ceiling and reports zero samples of latency.

## Frozen WAV

Lock WAV renders the active synth settings to mono 32-bit float RIFF audio. The WAV is stored in the plug-in state. Playback resamples it at the current host rate and continues through reverse, effects, output controls, filters, and limiting. Changing synth controls does not change a frozen hit until it is unlocked and captured again.

## Session state

The processor retains its host-facing parameter IDs and adds the ADSR and body-tone parameters. Missing parameters in older session state use current defaults. Old factory selections load as custom parameter states; they are not mapped to the new catalog. New V2 saves include the selected V2 program, lock mask, parameter tree, and any frozen WAV.
