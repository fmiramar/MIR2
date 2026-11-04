# MIRa implementation audit

Date: 2026-08-20

This is the historical eleven-class baseline audit. For the expanded library,
see `roadmap-status.md`, `implementation-log.md` and `THIRD_PARTY_NOTICES.md`.
The later auditory expansion includes an explicitly attributed SuperCollider
loudness contour table; the baseline no-table statement below is historical.

## Unified project decision

The MIRa and Modern Pitch Analysis plans overlap directly at `Pitch2`, common
frame/autocorrelation infrastructure, real-time policy, tests, and later tonal
features. They are therefore one separately releasable `MIRa` family. Modern
Pitch Analysis is a first-class subsystem, not a second repository that would
duplicate class names and kernels.

## Existing implementation review

The audit covered:

- core SuperCollider `Pitch`, `MFCC`, and `Onsets` classes and server sources;
- sc3-plugins `Tartini` and `Qitch` public classes, help, and source headers;
- installed core, sc3-plugins, FluCoMa-adjacent, Quark, and extension class
  namespaces available to the development installation.

The eleven version-0.1 public names were unoccupied at audit time. Abstract
language helpers use project-prefixed names: `MIRaFFTAnalyzer` and
`MIRaPitchAnalyzer`.

Core `Pitch` holds the last frequency when detection fails, whereas MIRa
defaults to zero plus zero confidence/voicing. Core `MFCC` has a historical
fixed representation; `MFCC2` exposes coefficient zero, band count, frequency
bounds, Mel mapping, spectrum convention, DCT normalization, and liftering.
Core `Onsets` provides several classic onset-detection functions; `Onsets2`
starts with explicit flux variants and a causal SuperFlux-style method whose
time state is expressed in seconds.

## Licensing and provenance

The project is GPL-3.0-or-later. Algorithm code was written from formulas and
papers listed in `ORIGINS.md`. No Essentia, librosa, FluCoMa, SuperCollider
analyzer, Tartini, or Qitch source was copied into this project. There are no
weights, datasets, or third-party lookup tables.

## Real-time design

- FFT/state buffers are allocated with SuperCollider's RT allocator only in
  constructors and released in destructors.
- Pitch autocorrelation uses a zero-padded radix-2 FFT plus prefix energies,
  avoiding a quadratic lag-by-sample loop.
- All callback loops have bounds fixed by validated initialization arguments.
- There is no callback file access, logging, model loading, or project-owned
  locking.
- FFT-chain access follows SuperCollider's buffer synchronization API. With the
  documented `LocalBuf` examples this synchronization performs no mutex lock;
  non-local FFT buffers may require the server's shared buffer lock.
- NaN input samples are replaced with zero before pitch analysis.

## Explicit version-0.1 limits

- `Pitch2` supports YIN and MPM only.
- `Periodicity` supports NSDF and YIN evidence only.
- `Voicing` is deterministic and model-free, not a calibrated speech
  classifier.
- `Onsets2` provides three initial methods rather than every method proposed by
  the long-term plan.
- No probabilistic candidate, SWIPE', polyphonic, predominant-melody, CQT,
  neural, rhythm, tonal, wavelet, or offline subsystem is claimed complete.

