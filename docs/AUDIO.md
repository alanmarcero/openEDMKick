# Audio checks

These are measurements from the unit tests, not listening tests. Render the example hits with:

```sh
cmake --build build --target openedmkick_tests --parallel
OEK_RENDER_DIR=/tmp/oek-audio ./build/openedmkick_tests '[metrics]'
```

That writes 32-bit float mono WAVs:

- `club-kick-f1.wav`: default club kick, F1, 420 ms tail, 48 kHz
- `club-kick-f1-plus7st.wav`: the same hit transposed +7 semitones
- `punch-click.wav`: short tail with punch, click, and sub drive up

## Default F1 club kick at 48 kHz

Pitch is the average interval between interpolated rising zero crossings from 50% to 80% of the tail, after the pitch envelope has landed.

| Check | Result |
| --- | --- |
| Tuned fundamental | 43.6535 Hz |
| Measured tail pitch | 43.6666 Hz |
| +7 st target | 65.4064 Hz |
| +7 st measured | 65.4123 Hz |
| Mean (DC) | about -7.4e-6 |
| Peak | -0.890 |
| Samples at full scale | 0 of 20160 |

The punch-click example reaches full scale for 12 samples. Those samples are the synth soft clip. The output limiter is a separate stage.

## Snare, clap, and hat layers (v1.2.0)

The layers mix into the kick voice. Level 0 is the default and leaves the dry kick sample-identical, including when the modifier knobs move. Older sessions omit the new parameter IDs and load that default.

| Layer | Level | Modifiers | What changes |
| --- | --- | --- | --- |
| Snare | `snare_level` | Tone (shell to wires), Decay (28–238 ms) | Pitched shell plus a band-passed wire buzz. Decay lengthens the ring. |
| Clap | `clap_level` | Spread (tight crack to a 4-hit flam), Tone (dark to bright) | Four staggered noise bursts. Spread is the gap between hits. |
| Hat | `hat_level` | Tone (chunky metal to thin chiff), Decay (16–356 ms, closed to open) | Six inharmonic partials plus a high noise chiff. Semitone offset tunes the metal. |

Tail Length still ends the whole voice, so a very short kick chops a long layer. Lock In bakes the current layer mix into the WAV. Changing a layer knob while locked does not update that WAV until you lock again.

`OEK_RENDER_DIR=... ./build/openedmkick_tests '[metrics]' writes audition files peak-matched to 0.89. That match is only in the files.

- `dry-kick.wav`
- `kick-plus-snare.wav`: level 0.80, tone 0.70, decay 0.45
- `kick-plus-clap.wav`: level 0.75, spread 0.60, tone 0.65
- `kick-plus-hat.wav`: level 0.70, tone 0.80, decay 0.28
- `kick-plus-layers.wav`: snare 0.55, clap 0.42, hat 0.38
- `layers-comparison.wav`: those five hits in that order, with 80 ms of silence after each

On the full 420 ms hit, absolute residual against the dry kick was about 513 (snare), 125 (clap), 316 (hat), and 388 (stacked). The clap number is smaller because the flam is short. The dry F1 metrics above are unchanged with the layers at zero.

## High pass and low pass

Both are two cascaded one-pole stages, 12 dB/oct, with no resonance and no resonance control. In the engine they run after the kick, the snare, clap, and hat layers, drive, crush, stutter, pump, gate, and output gain, and before the limiter.

High pass at 20 Hz and low pass at 20 kHz are off. The sample is unchanged, including on sessions saved before these parameters existed. Moving a cutoff eases in over about 15 ms.

`LOCK IN (WAV)` stores the hit before the filters. Playing that WAV still runs the current high pass and low pass. The waveform preview uses the same cutoffs.

## Regression checks

- Live synthesis matches `renderKickWaveform` for an uncached voice. The tail fade runs once.
- A locked 48 kHz WAV played at 96 kHz keeps the same fundamental.
- Positive pitch and tail curves still decay faster than negative curves.
- Retrigger continuity stays within 0.2 of the previous sample.
- The limiter reports 0 samples of latency, holds an impulse on the sample that exceeded the ceiling, and replaces NaNs.
- The bitcrusher output stays on its quantization grid.
- Auto Mode is silent while the transport is stopped and still accepts an audition trigger.
- A hot kick through drive, crush, pump, stutter, and the limiter stays finite and at or under the ceiling.
