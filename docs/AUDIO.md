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

## What the tests lock in

- Live synthesis matches `renderKickWaveform` for an uncached voice, so the real-time tail is not faded twice.
- A locked 48 kHz WAV played at 96 kHz keeps the same fundamental.
- Positive pitch and tail curves still decay faster than negative curves.
- Retrigger continuity stays within 0.2 of the previous sample.
- The limiter reports 0 samples of latency, holds an impulse on the sample that exceeded the ceiling, and replaces NaNs.
- The bitcrusher output stays on its quantization grid.
- Auto Mode is silent while the transport is stopped and still accepts an audition trigger.
- A hot kick through drive, crush, pump, stutter, and the limiter stays finite and at or under the ceiling.
