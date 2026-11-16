# openKicker v3.2.0 native editor screenshots

The four images are rendered by the compiled native JUCE editor at 1100 × 940. They show Advanced view with graphite panels, mint/amber controls, per-parameter padlocks, section locks, and visible current MIDI assignments.

- [Kick / Full ADSR](../assets/openKicker-kick.png): optional ADSR with 20-second release, a locked Dive time, and Harmonics assigned to CC35.
- [Layers](../assets/openKicker-layers.png): independent layer response, tuning, key/velocity zones, and padlocks.
- [Pattern](../assets/openKicker-pattern.png): shared ALL lane plus A–D lanes with ratchets, with separate clock/scene locks.
- [FX](../assets/openKicker-fx.png): reverse, bitcrush/downsample, stutter, and chopper MIDI activations; defaults CC20–25, Volume CC7, Expression CC11, and Brightness CC74 are visible beside their controls.

```sh
cmake --build build --target openedmkick_screenshots --config Release
```

The target builds `openedmkick_plugin_tests` and runs `[capture]` with `OEK_CAPTURE_DIR` set to this repository's `assets` directory. Existing PNGs are truncated and replaced. Normal CTest skips the capture-only case when that environment variable is absent. To run it directly, set `OEK_CAPTURE_DIR` to an absolute output directory and invoke `openedmkick_plugin_tests '[capture]'`.

The tests verify output size and PNG writing. Every final image is also visually inspected. These are native component snapshots, not promotional images or mockups.
