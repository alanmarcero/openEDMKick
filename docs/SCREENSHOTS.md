# openEDMKick v3.7.0 native editor screenshots

The images come from the compiled JUCE editor. Kick, held-note mode, Layers, and Pattern use the 1080 × 764 default window. Effects uses a taller window to show all advanced controls.

- [Kick](../assets/openEDMKick-kick.png) shows the first Uplifting preset, C2 waveform, locked Dive time, and Harmonics assigned to CC35.
- [Hold MIDI notes](../assets/openEDMKick-held.png) shows the MIDI envelope in the compact editor.
- [Layers](../assets/openEDMKick-layers.png) shows the mixer, pitch and filters, response, and MIDI zones.
- [Pattern](../assets/openEDMKick-pattern.png) shows the clock, scene, shared hits, and per-layer ratchets.
- [Effects](../assets/openEDMKick-fx.png) shows MIDI activations, filter sync, and output controls.

```sh
cmake --build build --target openedmkick_screenshots --config Release
```

The target builds `openedmkick_plugin_tests` and runs `[capture]` with `OEK_CAPTURE_DIR` set to this repository's `assets` directory. Existing PNGs are truncated and replaced. Normal CTest skips the capture-only case when that environment variable is absent. To run it directly, set `OEK_CAPTURE_DIR` to an absolute output directory and invoke `openedmkick_plugin_tests '[capture]'`.

The tests verify output size and PNG writing. Every final image is also visually inspected. These are native component snapshots, not promotional images or mockups.


## OpenVST Manager preview

OpenVST Manager shows `docs/screenshots/preview.png` from `main` (see `screenshotPath` in its catalog), and the GitHub release body and README hero point at the same file. This is the single image to keep current. Refresh it with the main editor capture whenever the UI changes, and commit it before tagging a release:

```sh
mkdir -p build/shots && OEK_CAPTURE_DIR="$PWD/build/shots" build/openedmkick_plugin_tests '[capture]'
cp build/shots/openEDMKick-kick.png docs/screenshots/preview.png
```
