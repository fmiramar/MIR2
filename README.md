# MIR2

MIR2 is a native SuperCollider music-information-retrieval family built from
published signal-processing methods. It combines modern replacements for
constrained historical analyzers with a Modern Pitch Analysis subsystem that
keeps periodicity, voicing, confidence, and octave ambiguity distinct.

The project belongs in Jumble SCience because its primary purpose is to
implement and document research techniques rather than port one upstream
codebase. MIR2 is complementary to FluCoMa: it concentrates on SC-native
real-time descriptors and modern legacy successors, while FluCoMa remains the
better fit for corpus, decomposition, and data-analysis workflows.

Version 0.1.0 establishes the shared engines and combined first sprint from
the MIR2 and Modern Pitch Analysis plans.

## Included analyzers

- `SpecCentroid2` reports the energy-weighted spectral center in Hz. It makes
  the magnitude/power convention, frequency bounds, FFT size, and silence
  result explicit rather than inheriting historical assumptions.
- `SpecFlatness2` reports the geometric-to-arithmetic mean ratio from 0 to 1.
  It uses a configurable logarithmic floor while returning exactly zero for a
  silent spectrum.
- `SpecPcile2` reports the frequency below a requested cumulative spectral
  fraction. It generalizes rolloff with explicit interpolation and spectrum
  conventions.
- `MFCC2` returns configurable Mel-frequency cepstral coefficients, including
  coefficient zero when requested. Slaney and HTK Mel mappings share a tested
  triangular-filter-bank and DCT-II engine.
- `FFTFlux2` reports normalized or absolute frame-to-frame spectral distance.
  It supports full or positive differences and allocates its previous-frame
  state only during unit construction.
- `Onsets2` returns a trigger and continuous onset strength. Its initial
  methods are full flux, positive flux, and a causal SuperFlux-style
  maximum-filtered log-spectrum difference with time-based threshold state.
- `PitchYIN` returns F0, CMNDF-derived confidence, and a voiced flag. It is a
  clean implementation of YIN using FFT autocorrelation to evaluate the
  difference function efficiently.
- `PitchMPM` returns F0, NSDF clarity, and a voiced flag. It implements the
  McLeod Pitch Method peak-selection strategy from the published description.
- `Periodicity` reports repeating-structure evidence from 0 to 1 without
  requiring the user to accept a final F0 decision.
- `Voicing` returns a binary decision and continuous model-free voicing
  evidence, combining periodicity, energy, and zero-crossing behavior.
- `Pitch2` is the general YIN/MPM facade and returns F0, confidence, voicing,
  and an explicit octave-competitor score. Its default MPM method can be
  changed symbolically at SynthDef construction.

All analyzers produce control-rate outputs. Pitch analyzers receive audio-rate
input internally and update once per configured hop. FFT analyzers consume a
normal SuperCollider FFT chain without changing spectral values; stateful FFT
analyzers require an explicit matching `fftSize` so callback-time allocation
is unnecessary.

## Build and install

Use a matching SuperCollider source checkout and an out-of-tree build:

```sh
cmake -S . -B build \
  -DSC_PATH=/path/to/supercollider \
  -DCMAKE_BUILD_TYPE=Release \
  -DSCSYNTH=ON -DSUPERNOVA=OFF -DSTRICT=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/SuperCollider/Extensions
```

Recompile the SuperCollider class library after installation. Open
`Guides/MIR2` for the family overview and runnable examples.

## Frame and silence policy

FFT size, frame size, output count, filter-bank size, algorithm selectors, and
hop fraction are initialization-time structure. Supported frame and FFT sizes
are powers of two from 256 through 8192; pitch frames must be long enough to
contain the requested low-frequency periods.

Silence produces zero frequency, confidence, voicing, periodicity, centroid,
percentile, flux, and onset strength. `SpecFlatness2` also returns zero on
silence. FFT descriptors naturally hold their most recent result between FFT
frames; pitch values are not held across failed analyses unless `Pitch2` is
given `hold: 1`.

## Scope and roadmap

This release intentionally stops at the two plans' first implementation
sprints. Probabilistic candidates, SWIPE', consensus, multipitch, predominant
melody, tonal/rhythm families, CQT/VQT, neural inference, and high-dimensional
buffer output remain later phases. No placeholder classes are installed for
unfinished algorithms.

Version 0.1.0 also does not claim numerical identity with legacy SC UGens,
FluCoMa, librosa, or Essentia. See `docs/AUDIT.md`, `ORIGINS.md`, and the help
guides for exact differences and provenance.

## License

MIR2 is licensed under GPL-3.0-or-later. It includes no Essentia or librosa
runtime, third-party model weights, datasets, or copied neural implementations.

