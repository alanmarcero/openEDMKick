# openKicker audio engine

openKicker v3.1 combines four independently shaped drum voices into one MIDI instrument. The engine remains a self-contained C++20 library without a JUCE dependency.

## Drum layers

MIDI triggers reach enabled layers whose inclusive key and velocity zones match. Shared Auto Mode notes address every enabled layer; explicit A–D lane notes address their own layer. Layers A–D each select Kick, Snare, Clap, or Hat and have their own level, pitch, decay, high-pass, low-pass, velocity curve, velocity timbre response, drive offset, tone scale, and kick sweep scale. Each layer's filters run before the voices are mixed. Layer A starts as an enabled kick; B, C, and D start muted with Snare, Clap, and Hat selected.

Kick layers use the main kick controls for their swept, harmonically shaped voice. Their layer decay scales the main decay. Snare layers combine a pitched body with band-limited noise. Clap layers use a short noise burst pattern. Hat layers combine bright noise with inharmonic metallic partials. Layer pitch transposes kick and snare notes and tunes the hat partials.

The incoming MIDI note sets the kick oscillator's equal-tempered fundamental around A4 = 440 Hz. Pitch sweeps start above that note and land on its tuned pitch. All matching layers receive the same MIDI note and incoming velocity; each applies its own velocity response. Auto Mode uses C2 and passes through the same zones. Note-off does not end a one-shot.

Velocity maps to a configurable amplitude exponent (default 1.45) and an independent timbre-response amount. Zero timbre response holds drive/transients at full-hit timbre; the default amount of one retains the original response. Pattern steps use their stored velocity through the same response.

## Factory kick balance

The factory kicks span Uplifting, Rolling, Progressive, Psy, Tech, and Classic. Upper harmonics fall away after the pitch dive, so the tail is the sub. Hat metal lasts a few milliseconds and the snare's pitched knock does not carry the decay. Held-note playback remains opt-in.

The attack sub runs at the MIDI fundamental. As the pitch dive passes through its last half-octave, a smooth crossfade moves the clean sub onto the body oscillator's sine phase. The settled sub and body reinforce each other across pitches, including in held-note mode. Reverse uses the same blend evaluated in source time. The knock boost applies its requested gain once and preserves unity gain away from its centre frequency.

## Pattern clock

The sequencer has four independent pattern scenes. Each stores a fill selection, bar count, grid, tail trim, and bitcrush ramp. The 28 trance fills populate the shared ALL lane with velocity and optional reverse cues. Four additional lanes target layers A–D. Each note can repeat 1–8 times evenly within its length; simultaneous explicit layer notes override shared notes for that layer. Click adds/removes a note, and Shift-click cycles ratchets. Auto Mode triggers use C2; external MIDI triggers use their incoming note.

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

Solo, linear pan, and polarity operate per layer before the stereo mix. Center pan retains the legacy level in both channels; mono output averages the stereo mix. The stereo engine applies drive saturation, bitcrushing, host-synced beat repeat, rhythmic filter pump, tempo gate, output gain and MIDI Volume/Expression. It then applies the host-synced or static high-pass and low-pass filters, MIDI Brightness ceiling, and optional zero-latency peak limiter.

## Session state

The processor keeps the openKicker plug-in identity and saves a factory catalog version alongside the complete parameter state. A session from an older bank restores its parameter values as a custom sound. Layer settings and host-sync parameters are saved with the session and are available for automation. New layer parameters append to the existing parameter list with AU version hint 3. All four scenes' custom lane notes and ratchets are stored in a separate LayerPatterns state child. Editor edits publish precompiled schedules through an atomic triple buffer; the callback swaps pattern storage without taking the editor mutex. Supported MIDI input is bounded to 65,535 events plus an audition per callback; excess input is ignored. Older sessions retain matching parameter values and use defaults for the added layer and sync controls.


## Full ADSR and note tracking

Full ADSR is opt-in with 0–10-second attack/decay, 0–1 sustain, and 0–20-second release. Eight fixed MIDI slots retain note/channel ownership; one-shot Auto Mode and audition use a separate slot. Matching note-offs enter release at their sample offset. Enabling ADSR preserves existing one-shot hits, and disabling it releases held gated voices. Release also advances while output is bypassed. Host tail reporting follows remaining release and repeat-window time, plus a finite shared-filter/sample-hold allowance. Native host bypass still consumes MIDI note-offs and advances releases while muting the output.

C2 is the synthesis frequency reference. Main/layer transpose, knock, transient tone, metal partials, and tuned body/noise/tail filters follow MIDI pitch with Nyquist safety clamps. White noise generation remains broadband. The disabled ADSR path retains the reference one-shot sound.

## Performance FX and parameter routing

Default activation bindings are Reverse CC20, HP CC21, LP CC22, Crush CC23, Stutter CC24, Chopper CC25, Volume CC7, Expression CC11, and Brightness CC74. HP/LP MIDI activation is distinct from each automated sync switch. Crush/Stutter/Chopper activation uses configured nonzero Mix/Depth or full activation at zero; automated amounts remain independent when MIDI activation is off.

Reverse setup uses a fixed 128-point phase integration and then renders backward source time per sample, with a 3 ms end fade. There is no full-hit rendering or reverse sample buffer in note-on handling. This preserves the rising reverse character while intentionally changing sample-exact filtered/noise reversal. Reverse is retimed when tempo changes and stops at its PPQ target; performance reverse and stutter use the next quarter-note boundary, including a full beat for on-beat notes.

The router has stable parameter-ID metadata and fixed per-parameter atomic value mirrors. Incoming CCs update mirrors/DSP without APVTS or host-notification writes. The engine saves/restores current-sample mirrors around the controller prepass so Auto Mode sees current CC state while incoming notes preserve original order. Limiter/ceiling changes apply per sample. Existing sorted event cursors remain linear in event count. Temporary sequencer CC choices do not overwrite stored custom-note patterns.

Per-parameter/section locks are consulted only during factory recall. Full state restore replaces project values and routing metadata directly. User controls use ParameterAttachment callbacks with silent UI mirror updates, so editing back to an unchanged host value still clears a CC override.
