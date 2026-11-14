# V3.1 audio engine

openKicker v3.1 combines four independently shaped drum voices into one MIDI instrument. The engine remains a self-contained C++20 library without a JUCE dependency.

## Drum layers

Each MIDI or Auto Mode trigger plays every enabled layer. Layers A–D each select Kick, Snare, Clap, or Hat and have their own level, pitch, decay, high-pass, and low-pass settings. Each layer's filters run before the voices are mixed. Layer A starts as an enabled kick; B, C, and D start muted with Snare, Clap, and Hat selected.

Kick layers use the main kick controls for their swept, harmonically shaped voice. Their layer decay scales the main decay. Snare layers combine a pitched body with band-limited noise. Clap layers use a short noise burst pattern. Hat layers combine bright noise with inharmonic metallic partials. Layer pitch transposes kick and snare notes and tunes the hat partials.

The incoming MIDI note sets the kick oscillator's equal-tempered fundamental around A4 = 440 Hz. Pitch sweeps start above that note and land on its tuned pitch. All enabled layers receive the same MIDI note and velocity; Auto Mode uses C2. Note-off does not end a one-shot.

Velocity maps to a curved level response and also changes kick drive and transient intensity. Pattern steps use their stored velocity through the same response.

## Pattern clock

The sequencer has four independent pattern scenes. Each stores a fill selection, bar count, grid, tail trim, and bitcrush ramp. The 28 trance fills create a one-lane trigger pattern with velocity and optional reverse cues. The piano roll changes trigger position and length only. Auto Mode triggers use C2; external MIDI triggers use their incoming note.

Host sync uses host PPQ, tempo, and meter. Internal clock uses the selected tempo and meter. Tail trim estimates the time to the next pattern hit and reduces the current kick tail for fast patterns. It only applies to Auto Mode triggers.

## Host-synced effects and MIDI CC

The reverse mode, high-pass sync switch/rate, and low-pass sync switch/rate are plug-in parameters that hosts can automate. HP and LP sync rates range from 1/4 to 1/64. The high-pass sweep opens from 20 Hz to its selected cutoff (or 8 kHz when the cutoff is bypassed). The low-pass sweep closes from 20 kHz to its selected cutoff (or 200 Hz when bypassed). Both follow host PPQ and the selected musical division.

MIDI CC values activate the matching mode on every MIDI channel. Values 64–127 enable an effect; values 0–63 disable its CC activation. The mode parameter still controls its independently automated state.

| CC | Effect |
| --- | --- |
| 20 | Reverse; uses Beat End when Reverse mode is Off |
| 21 | Host-synced high-pass sweep |
| 22 | Host-synced low-pass sweep |
| 7 | MIDI volume trim |
| 11 | MIDI expression trim |
| 74 | Brightness / low-pass ceiling |
| 120 | All Sound Off |
| 121 | Reset volume, expression, brightness, and CC20–22 activations |
| 123 | Ignored; one-shot hits continue |

## Effect path

After the layered voices are mixed, the engine applies drive saturation, bitcrushing, host-synced beat repeat, rhythmic filter pump, tempo gate, output gain and MIDI Volume/Expression. It then applies the host-synced or static high-pass and low-pass filters, MIDI Brightness ceiling, and optional zero-latency peak limiter.

## Session state

The processor keeps the openKicker plug-in identity and v3 factory bank. Layer settings and host-sync parameters are saved with the session and are available for automation. Older sessions retain matching parameter values and use defaults for the added layer and sync controls.
