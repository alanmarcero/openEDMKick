# Audio checks

These are measurements from the unit tests, not listening tests. Render the example hits with:

```sh
cmake --build build --target openedmkick_tests --parallel
OEK_RENDER_DIR=/tmp/oek-audio ./build/openedmkick_tests '[metrics]'
```

That writes 32-bit float mono WAVs:

- `club-kick-f1.wav` — default club kick, F1, 420 ms tail, 48 kHz
- `club-kick-f1-plus7st.wav` — the same hit transposed +7 semitones
- `punch-click.wav` — short tail with punch, click, and sub drive up

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

The punch-click example reaches full scale for 12 samples. That is the soft clip on a hot transient, not the output limiter.

## Snare, clap, and hat layers (v1.2.0)

These are part of the kick voice, not separate drum instruments. Level 0 is the default and leaves the dry kick sample-identical, including when the modifier knobs are moved. Older sessions omit the new parameter IDs and load that default.

| Layer | Level | Modifiers | What changes |
| --- | --- | --- | --- |
| Snare | `snare_level` | Tone (shell to wires), Decay (28–238 ms) | Pitched shell plus a band-passed wire buzz. Decay lengthens the ring. |
| Clap | `clap_level` | Spread (tight crack to a 4-hit flam), Tone (dark to bright) | Four staggered noise bursts. Spread is the gap between hits. |
| Hat | `hat_level` | Tone (chunky metal to thin chiff), Decay (16–356 ms, closed to open) | Six inharmonic partials plus a high noise chiff. Semitone offset tunes the metal. |

Tail Length still ends the whole voice, so a very short kick chops a long layer. Lock In bakes the current layer mix into the WAV. Changing a layer knob while locked does not update that WAV until you lock again.

Audition files written by `OEK_RENDER_DIR=... ./build/openedmkick_tests '[metrics]'` are peak-matched to 0.89 so they can be compared. The plugin does not do that matching.

- `dry-kick.wav`
- `kick-plus-snare.wav` — level 0.80, tone 0.70, decay 0.45
- `kick-plus-clap.wav` — level 0.75, spread 0.60, tone 0.65
- `kick-plus-hat.wav` — level 0.70, tone 0.80, decay 0.28
- `kick-plus-layers.wav` — snare 0.55 / clap 0.42 / hat 0.38
- `layers-comparison.wav` — those five hits in that order, with 80 ms of silence after each

On the full 420 ms hit, absolute residual against the dry kick was about 513 (snare), 125 (clap), 316 (hat), and 388 (stacked). The clap number is smaller because the flam is short. The dry F1 metrics above are unchanged with the layers at zero.

## What the tests lock in

- Live synthesis matches `renderKickWaveform` for an uncached voice, so the real-time tail is not faded twice.
- A locked 48 kHz WAV played at 96 kHz keeps the same fundamental.
- Positive pitch and tail curves still decay faster than negative curves.
- Retrigger continuity stays within 0.2 of the previous sample.
- The limiter reports 0 samples of latency, holds an impulse on the sample that exceeded the ceiling, and replaces NaNs.
- The bitcrusher output stays on its quantization grid.
- Auto Mode is silent while the transport is stopped and still accepts an audition trigger.
- A hot kick through drive, crush, pump, stutter, and the limiter stays finite and at or under the ceiling.
