# openKicker v3.1

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

openKicker is a MIDI-tuned kick and layered drum synthesizer. Version 3.1 adds four independently shaped sound layers and host-synced filter effects while keeping the tuned kick voice, pattern sequencer, and existing preset bank.

## Sound design

- **Dive:** Shape the pitch sweep and opening snap above the played note. The sweep always lands on that exact MIDI note.
- **Body:** Add upper harmonics, resonant knock, saturation, grit, tone shaping, and a clean sub layer for a harder trance impact.
- **Transient:** Choose a beater, noise, tick, or analog click, then set its tone and level.
- **Tail:** Shape the one-shot attack, hold, decay, curve, low cut, and clipping.
- **Waveform and note readout:** See the current MIDI note and its equal-tempered frequency alongside the live sound preview.
- **48 factory kicks:** The full bank is rebuilt for the new engine, from tight club impacts to longer, heavier trance tails.
- **Simple and advanced views:** Start with the controls that matter most, then open the full set for detailed sound design.

## Drum layers

The **Layers** workspace has four pages, A through D. Each layer can be enabled or muted and set to Kick, Snare, Clap, or Hat. Set the level, pitch, decay, high-pass, and low-pass on each page. The filters run on their own layer before the voices are mixed into one hit. Layer A starts as an enabled kick; B, C, and D start muted with Snare, Clap, and Hat selected.

Each layer has inclusive MIDI key and velocity limits. Reversed limits reject all notes. Velocity curve sets the amplitude exponent (0.1–4, with the original 1.45 response as default); Velocity timbre blends between fixed full-hit tone and velocity-sensitive drive/transients. These settings act independently on each layer.

The pitch control transposes its layer. Tone scale shifts its body/transient filter frequencies. Drive offset adds to the main kick drive, and shapes snare, clap, and hat voices independently. Sweep scale adjusts a kick layer's pitch dive and snap. Neutral offsets retain the main synthesis settings; layer decay still scales the main kick decay.

Solo auditions enabled layers; multiple solos can play together. Pan places each layer before the stereo FX, and Invert reverses its polarity. Center pan preserves the original dual-mono level. Mono output averages the stereo layer mix.

## MIDI

The kick voice follows the incoming note's equal-tempered pitch, including its octave. For example, MIDI A4 (note 69) plays at 440 Hz; A3 (note 57) plays at 220 Hz. The layer pitch control transposes that note for an individual layer. Note-off does not stop a one-shot. An audition click plays C2.

Velocity shapes the hit level and kick drive, so soft hits have less weight and hard hits gain extra bite. MIDI input is omni-channel. The callback accepts up to 65,535 supported MIDI events plus one audition per block; excess events are ignored to keep storage bounded. CC20–22 activate host-synced FX; each stays active until its controller receives a value below 64. Performance CC values are not saved. The corresponding plug-in controls are available for automation:

| CC | Name | Effect |
| --- | --- | --- |
| 7 | Volume | Output trim multiplied by the Output control |
| 11 | Expression | Additional output trim |
| 20 | Sync Reverse | Enables reverse hits. Uses Beat End when Reverse mode is Off. |
| 21 | Sync High Pass | Enables the host-synced high-pass sweep. |
| 22 | Sync Low Pass | Enables the host-synced low-pass sweep. |
| 74 | Brightness | Sets the effective low-pass ceiling |
| 120 | All Sound Off | Stops the current hit |
| 121 | Reset Controllers | Resets Volume, Expression, Brightness, and CC20–22 activations |
| 123 | All Notes Off | Ignored; one-shot hits continue |

## Pattern sequencer

Four independent pattern scenes, A/B/C/D, have separate length, fill, grid, tail trim, and bitcrush ramp. Each piano roll contains an **ALL** lane plus separate **A–D layer lanes**. Factory patterns and older sessions use ALL, triggering every enabled layer at the step velocity. Click a layer lane to add or remove a hit for that layer only; Shift-click an existing hit to cycle its ratchets from 1 to 8. Ratchets divide the hit's grid length evenly, including triplets. Explicit layer hits take priority over simultaneous ALL hits for the same layer. Lane edits and ratchets are saved with the session. Changing a scene's fill or length reloads its factory pattern. Internally generated hits use C2; incoming MIDI notes still trigger at their exact pitch. The 28 trance fills and every preset have been authored for the current kick engine.

Auto Mode follows host tempo, meter, and playhead, or runs from its own tempo and time signature. Tail trim shortens fast rolls in Auto Mode. It does not change incoming MIDI hits.

## Effects

The FX page includes host-synced reverse, high-pass and low-pass sweeps, drive, bitcrushing, beat stutter, gate, filter pump, output trim, peak meter, and a zero-latency limiter. Reverse mode and the HP/LP sync switches and rates are automatable. CC20, CC21, and CC22 also activate their matching effects during performance. Filter sync rates range from 1/4 to 1/64.

## Sessions and compatibility

The plug-in identity remains openKicker. Version 3.1 keeps the v3 preset catalog and adds saved layer and sync-effect parameters. Older sessions load matching controls and use the new default layer setup for parameters they do not contain.

## Build

The project uses C++20 and JUCE 8.0.9. It builds VST3, AU, LV2, and Standalone formats on macOS; VST3, LV2, and Standalone on Windows and Linux.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target OpenEDMKick_Standalone --config Release --parallel
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Source layout

- `Source/Core`: Synth, Auto Mode sequencer, effects, and audio engine.
- `Source/Plugin`: Parameters, presets, JUCE processor, custom controls, and editor pages.
- `Tests`: Existing Catch2 checks used by the GitHub Actions build.
- `docs`: Audio behavior, compatibility, and design notes.
