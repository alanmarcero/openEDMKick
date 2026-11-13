# openKicker v3

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

openKicker is a chromatic trance kick synthesizer and pattern instrument. Version 3 rebuilds the kick voice, sound design controls, sequencer lane, preset bank, and editor around one rule: every MIDI note sets the kick's root pitch and octave.

## Sound design

- **Dive:** Shape the pitch sweep and opening snap above the played note. The sweep always lands on that exact MIDI note.
- **Body:** Add upper harmonics, resonant knock, saturation, grit, tone shaping, and a clean sub layer for a harder trance impact.
- **Transient:** Choose a beater, noise, tick, or analog click, then set its tone and level.
- **Tail:** Shape the one-shot attack, hold, decay, curve, low cut, and clipping.
- **Waveform and note readout:** See the current MIDI note and its equal-tempered frequency alongside the live sound preview.
- **48 factory kicks:** The full bank is rebuilt for the new engine, from tight club impacts to longer, heavier trance tails.
- **Simple and advanced views:** Start with the controls that matter most, then open the full set for detailed sound design.

## MIDI

Every note-on plays at its exact equal-tempered pitch, including its octave. For example, MIDI A4 (note 69) plays at 440 Hz; A3 (note 57) plays at 220 Hz. There is no fixed-pitch mode, pitch selector, octave offset, fine tune, or pitch lock. Note-off does not stop a one-shot. An audition click plays C2.

Velocity scales the hit. MIDI input is omni-channel. Performance controllers are not saved and do not write automation parameters:

| CC | Name | Effect |
| --- | --- | --- |
| 7 | Volume | Output trim multiplied by the Output control |
| 11 | Expression | Additional output trim |
| 74 | Brightness | Sets the effective low-pass ceiling |
| 120 | All Sound Off | Stops the current hit |
| 121 | Reset Controllers | Resets Volume, Expression, and Brightness |
| 123 | All Notes Off | Ignored; one-shot hits continue |

## Pattern sequencer

Four independent pattern scenes, A/B/C/D, each hold a one-lane kick sequence with separate length, fill, grid, tail trim, and bitcrush ramp. The piano roll edits trigger placement and velocity without changing pitch. Internally generated Auto Mode hits use C2; incoming MIDI notes still trigger at their exact pitch. The 28 trance fills and every preset have been authored for the new engine.

Auto Mode follows host tempo, meter, and playhead, or runs from its own tempo and time signature. Tail trim shortens fast rolls in Auto Mode. It does not change incoming MIDI hits.

## Effects

The FX page includes host-synced reverse, drive, bitcrushing, beat stutter, gate, filter pump, high-pass and low-pass filters, output trim, peak meter, and a zero-latency limiter. Simple view keeps the main controls visible; advanced view exposes timing and tone details.

## Sessions and compatibility

The plug-in identity remains openKicker. Version 3 replaces the old sound and preset catalog; older sessions load their matching controls where possible and are treated as custom settings. Old factory program numbers are not mapped to the new bank. Frozen WAV, pitch-lock, fixed-pitch, and root-selection state are no longer part of the instrument.

## Build

The project uses C++20 and JUCE 8.0.9. It builds VST3, AU, LV2, and Standalone formats on macOS; VST3, LV2, and Standalone on Windows and Linux.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target OpenEDMKick_Standalone --config Release --parallel
```

## Source layout

- `Source/Core`: Synth, Auto Mode sequencer, effects, and audio engine.
- `Source/Plugin`: Parameters, presets, JUCE processor, custom controls, and editor pages.
- `Tests`: Existing Catch2 checks used by the GitHub Actions build.
- `docs`: Audio behavior and v3 design notes.
