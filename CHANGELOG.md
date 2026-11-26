# Changelog

## v3.10.5

- Right-click an EQ note to pick a note in that band.

## v3.10.4

- Completes the openKicker rename and aligns the suite controls and tests.
- Aligns the JUCE 8 license and hardens CI.

## v3.10.3

- **Note-Snapped EQ:** Snap all three EQ bands to chromatic notes (A-G with sharps) across two-octave ranges (`E1` to `E3`, `B4` to `B6`, `A#7` to `A#9`), with octave markers on the outer ring and note names in the EQ readouts.

## v3.10.2

- **Parameter Locking:** Add per-parameter locking across every plugin parameter, persisted in session state and preserved across preset loads and randomization.

## v3.10.1

- **UI & Suite Fixes:** UI polish, full-size kick knobs, truncated preset saves, double-click resets, unit formatting and typed-value entry, per-slot A/B undo, dBFS post-limiter meter, and screen-reader titles.

## v3.10.0 — 2026-11-20

- **Zero-Delay Feedback SVF Voice & Tone Filters**: Replaced RBJ biquads with TPT/ZDF state-variable filters so fast cutoff modulation preserves steady DC state.
- **Analytic Pitch Integration & Phase-Locked Sub**: Analytic cycle integration keeps fast snap pitch sweeps phase-accurate across sample rates while keeping the sub oscillator phase-locked through the sweep transition.
- **Oversampled ADAA Body Saturation**: Added antiderivative anti-aliased saturation on driven kick bodies for warm low-mid even harmonics.
- **Modal Beater Transient & Causal Reverse Cache**: Upgraded the beater click to multi-partial inharmonic modal synthesis and renders causal forward filters into a reverse playback cache.
- **Coupled Snare & Irrational-Ratio Hat Layers**: Synthesizes coupled dual-membrane snare modes with comb wire rattle and irrational square-ring hat sidebands without phase-wrap clicks.
- **Lookahead True-Peak Limiter (64 samples) & Band-Limited Bitcrusher**: Added 4x polyphase inter-sample-peak detection with adaptive dual-stage release and pre-filtered bitcrusher anti-aliasing.
- **Mid/Side Allpass Stereo Widener**: 4-stage Thiran-interpolated Schroeder allpass decorrelation on the Side channel with sub-bass mono coherence.
