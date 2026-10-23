# Development changes (unreleased)

- Added classical Onsets methods, a quarter-tone SuperFlux frontend, actual
  median and causal peak picker, quiet-signal suppression and diagnostic
  comparisons with the previous implementation and native OnsetsDS.
- Added extended spectral, auditory, advanced pitch, temporal, stereo and
  feature-dynamics families with meaningful audible examples.
- Added periodic Haar/db1–db4 wavelet analysis and reconstruction.
- Fixed control-output initialization and stopped analysis readers from
  mutating shared FFT coordinate representations.
- Expanded independent numerical checks and installed example verification.
- Full-library work is ongoing; see docs/roadmap-status.md.

# Changelog

## 0.1.0 — 2026-08-20

- Added shared FFT, spectral-statistics, Mel/DCT, frame, autocorrelation,
  difference, CMNDF, NSDF, and voicing kernels.
- Added `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`,
  and `Onsets2`.
- Added the unified modern pitch family: `PitchYIN`, `PitchMPM`, `Pitch2`,
  `Periodicity`, and `Voicing`.

