# V3 audio engine

openKicker v3 replaces the kick voice, sequencer patterns, and sound design controls. The engine remains a self-contained C++20 library without a JUCE dependency.

## MIDI-tuned kick voice

The incoming MIDI note directly sets the oscillator's fundamental using equal temperament around A4 = 440 Hz. Pitch sweeps start above that note and land on it; no fixed root, octave offset, fine tuning, or pitch-lock stage can override incoming MIDI. An A4 therefore lands at 440 Hz and A3 at 220 Hz.

The oscillator adds second through fourth harmonics to the fundamental. Resonant knock, several saturation curves, a decaying grit stage, tone shaping, and a parallel clean sub component give the body more midrange weight than a sine-only kick. The transient has beater, noise, tick, and analog modes. An attack, hold, decay, and curve shape the one-shot amplitude; MIDI note-off does not end a hit.

The editor preview and live voice share `renderKick`/`KickVoice` synthesis. The waveform page reports the latest MIDI note and its frequency. Audition uses C2.

## Pattern clock

The sequencer has four independent slots. Each stores a fill selection, bar count, grid, tail trim, and bitcrush ramp. The 28 trance fills create a single-pitch trigger lane with velocity and optional reverse cues. The piano roll changes trigger position and length only. Generated Auto Mode hits use C2; external MIDI triggers always use their incoming note.

Host sync uses host PPQ, tempo, and meter. Internal clock uses the selected tempo and meter. Auto Mode triggers use C2 because they are generated without a MIDI note. Tail trim estimates the time to the next pattern hit and reduces the current tail for fast patterns. It only applies to Auto Mode triggers.

## Effect path

Reverse samples the synthesized voice and schedules it against the selected musical boundary. The remaining signal path is:

1. Drive saturation.
2. Sample-rate and bit-depth reduction, blended with the dry signal.
3. Host-synced beat repeat, blended with the dry signal.
4. Rhythmic low-pass pump.
5. Tempo gate.
6. Output gain and MIDI Volume/Expression.
7. Smoothed high-pass and low-pass filters, plus MIDI Brightness ceiling.
8. Optional zero-latency peak limiter.

The high-pass and low-pass each use two cascaded one-pole stages. The effect returns the input at the 20 Hz high-pass and 20 kHz low-pass bypass points. The limiter applies gain reduction on the sample that crosses the ceiling and reports zero samples of latency.

## Session state

The processor preserves the plug-in identity. New v3 state stores the selected v3 program, current parameters, and four pattern slots. Older state is loaded using matching parameter IDs where available and marked as a custom sound. Frozen WAV data and old pitch-lock state are not restored.
