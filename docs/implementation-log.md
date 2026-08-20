# MIR2 implementation log

Date: 2026-08-20
Target: 0.1.0 unified first sprint

Both agentic plans were read in full before implementation. Their overlapping
`Pitch2`, frame, autocorrelation, real-time, documentation, and test policies
were resolved as one standalone `03-jumble-science/MIR2` project. All project
writes remained inside that directory while other agents worked elsewhere in
`03-jumble-science`.

## Implemented scope

Shared kernels:

- radix-2 FFT and zero-padded autocorrelation;
- spectral centroid, flatness, percentile, and flux formulas;
- Slaney/HTK Mel mappings, triangular banks, and DCT-II;
- fixed trailing frame/ring-buffer scheduling;
- difference, CMNDF, NSDF, local extrema, parabolic refinement, and octave
  relation analysis;
- deterministic energy/periodicity/zero-crossing voicing evidence.

Public version-0.1 analyzers:

- `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`, and
  `Onsets2`;
- `PitchYIN`, `PitchMPM`, `Periodicity`, `Voicing`, and `Pitch2`.

This is the union of MIR2's stated first public milestone and Modern Pitch
Analysis's recommended first agent sprint. No future class name is installed
as a placeholder.

## Integration findings

Installed-server tests caught two SuperCollider-specific issues before
handoff. First, a control-rate UGen's calculation-rate sample rate is not the
audio input sample rate, so every analysis conversion now uses the world's
full audio rate. Second, a control-rate analyzer receiving an audio-rate input
is called once per server block but must explicitly consume the full audio
input block; the frame feeder now uses the full-rate buffer length.

Integration tests use dedicated ports 57145 through 57148 to avoid interfering
with other concurrent SuperCollider work.

## Real-time audit

Persistent pitch, FFT-flux, and onset storage is allocated with `RTAlloc` only
from constructors and freed only from destructors. Calculation callbacks do
not allocate, free, print, open files, load models, or acquire project-owned
locks. All algorithmic loops are bounded by literal frame, FFT, band, or output
counts validated at SynthDef construction.

FFT-chain access uses SuperCollider's required buffer synchronization. With
the documented `LocalBuf` pattern it performs no mutex acquisition; a non-local
shared FFT buffer may require the server's buffer lock.

## Verification

The final source state passed:

- strict AppleClang scsynth and supernova builds;
- strict MinGW Windows x86-64 cross-build;
- AddressSanitizer and UndefinedBehaviorSanitizer reference tests;
- C++ FFT inversion, spectral formulas, filter-bank/DCT, 44.1/48/96 kHz pitch,
  256-through-8192 frame, missing-fundamental, silence, noise, finite-guard,
  confidence, and octave-ambiguity tests;
- installed class compilation and exact class-origin collision checks;
- compilation of all test scripts and all 12 parenthesized help examples;
- forced SCDoc indexing and rendering of 11 class pages plus three guides with
  no warning or error;
- an installed scsynth smoke test covering all public analyzers;
- installed silence, DC, impulse, noise, and legal-extreme-parameter safety;
- an installed supernova smoke test;
- a nine-case NRT matrix at 44.1, 48, and 96 kHz with block sizes 1, 64, and
  256. YIN, MPM, `Pitch2`, periodicity, and voicing stayed inside their
  deterministic sine-test bounds in every case.

The local 48 kHz/64-sample-block benchmark measured approximate incremental
server CPU per instance of 0.17 for the four-descriptor spectral core, 0.19
for 13-coefficient `MFCC2`, 0.17 for SuperFlux-style `Onsets2`, 1.75 for
2048-sample YIN or MPM, and 4.91 for the combined `Pitch2` + `Periodicity` +
`Voicing` family. These are local comparative figures, not portable targets.

Both scsynth and supernova variants were installed into the local
SuperCollider user extension directory. Hosted GitHub jobs and publication
were not run.

## Explicitly deferred

- probabilistic candidates, pYIN tracking, SWIPE', consensus, and pitch
  salience;
- multipitch, predominant melody, and contour tracking;
- broader spectral, auditory/cepstral, tonal, rhythm, temporal, stereo,
  wavelet, CQT, and VQT families;
- all neural inference and model packaging.

These remain roadmap phases requiring their own paper-based implementation,
tests, CPU evaluation, and any applicable code/model/data license review.

