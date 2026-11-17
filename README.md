# openKicker v3.3.1

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

openKicker makes pitched trance kicks and layered drum hits. Play it from a MIDI keyboard or sequence. MIDI notes set the kick pitch and octave. Add up to four kick, snare, clap, or hat layers to create a complete hit.

## Shape the kick

Use **Dive** to set the pitch drop and opening snap. Shape the **Body** with harmonics, knock, saturation, grit, tone, and sub weight. Choose a **Transient** such as a beater, noise, tick, or analog click. **Tail** controls set the one-shot envelope, low cut, and clipping. A live waveform and note display show the sound, MIDI note, and frequency. Simple View keeps the main controls in reach; Advanced View adds detailed controls.

The 48 factory kicks suit clean, bright, bass-forward trance. The default sound is a one-shot. Enable **Full ADSR** to hold the kick while a MIDI note is held and release it with the matching note and channel. Full ADSR also gates generated snare, clap, and hat tails. Audition and the pattern sequencer continue to play one-shots.

## Add drum layers

The **Layers** pages A–D each add a Kick, Snare, Clap, or Hat voice. Set level, pitch, decay, and filters for each layer. Key and velocity ranges choose which notes activate it. Set the velocity response and timbre, then adjust tone, drive, and pitch-sweep offsets. Solo, pan, or invert layers to shape the mix.

## Play and automate

Velocity changes the hit level and tone. Notes follow their MIDI pitch; the layer pitch control can transpose each voice. Use the MIDI page to assign or clear CC controls for parameters. The default CCs control output volume (7), expression (11), reverse (20), high-pass sync (21), low-pass sync (22), crush (23), stutter (24), chopper (25), and brightness (74). CC 120 stops the sound and effect tails. CC 121 resets controller overrides. In Full ADSR, use matching note-offs to release held notes.

Protect a section with its lock while browsing presets. Locked controls stay editable, and the lock state saves with the project.

## Make patterns

The pattern sequencer has four scenes, A–D. Each scene has its own pattern, length, fill, grid, tail trim, and bitcrush ramp. Use the ALL lane to trigger every enabled layer or edit separate lanes for each layer. Add up to eight ratchets to a step. Auto Mode follows the host tempo, meter, and playhead, or runs at a tempo and meter you set. It can play the 28 built-in trance fills.

## Add effects

Use the Effects page for reverse, high-pass and low-pass sweeps, drive, bitcrushing, stutter, gate, filter pump, and output trim. The peak meter and limiter help control output. Reverse, stutter, and filter sweeps can follow host timing. MIDI CCs can activate the glitch effects while you play.

## Screenshots

![Kick controls, Full ADSR, and preset locks](assets/openKicker-kick.png)

![Layer response, zones, and routing controls](assets/openKicker-layers.png)

![Shared and per-layer pattern lanes](assets/openKicker-pattern.png)

![Glitch MIDI controls and CC bindings](assets/openKicker-fx.png)

## Formats and license

VST3, AU, LV2, and Standalone formats are available on macOS. Windows and Linux versions include VST3, LV2, and Standalone. Older openKicker sessions retain their supported settings; new controls use their default values when an older session does not contain them.

MIT License. See [LICENSE](LICENSE).
