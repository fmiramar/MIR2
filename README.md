# MIR2

MIR2 is a native SuperCollider library for music information retrieval and
analysis-driven synthesis control. It belongs in Jumble SCience because its
algorithms implement published research techniques rather than wrap one upstream
library. It complements FluCoMa's corpus and data workflows.

This is an unpublished development expansion of version 0.1 covering all primary
classical roadmap families and the evaluated native research extensions.
The separate, optional [MIR2Neural extension](neural/README.md) adds native PESTO
pitch inference. Recording-based quality evaluation and release qualification
remain explicit in the [current status](docs/roadmap-status.md).

## Included analyzers

The development tree provides 135 public classes.

MIR2Neural separately provides `PitchPESTO`, a single-pitch estimator using the
PESTO v2 variable-Q frontend and convolutional network. Its CPU implementation
requires an explicitly exported model buffer and adds no dependency to MIR2's
default build.

- **Core**: `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`, `Onsets2`, `PitchYIN`, `PitchMPM`, `Periodicity`, `Voicing`, `Pitch2`.
- **Auditory filters and cepstra**: `BarkBands`, `ERBBands`, `BFCC`, `LFCC`, `GammatoneBands`, `GFCC`, `Loudness2`.
- **Constant-Q analysis**: `CQT`, `VQT`, `CQCC`, `ChromaCQT`.
- **Feature dynamics**: `FeatureDelta`, `FeatureDeltaDelta`, `FeatureMedian`, `FeatureSmooth`.
- **Harmonic analysis**: `HarmonicPeaks`, `Inharmonicity`, `Tristimulus`, `OddEvenRatio`, `HarmonicEnergyRatio`, `HarmonicCentroid`, `HarmonicDeviation`, `HarmonicSpread`, `HarmonicVariation`, `Noisiness`, `SensoryDissonance2`.
- **Onset evidence**: `FFTComplexDev2`, `FFTPhaseDev2`, `FFTMKL2`, `FFTFluxPos2`.
- **Power-normalized cepstra**: `PNCC`.
- **Advanced pitch**: `FundamentalCandidates`, `PitchProb`, `PitchAmbiguity`, `PitchSWIPE`, `PitchConsensus`, `PitchSalienceBuf`, `PitchSalience`, `MultiPitch`, `PredominantPitch`, `PitchContour`, `PitchStability`, `PitchDeviation`, `Vibrato2`.
- **Rhythm analysis**: `Tempogram`, `FourierTempogram`, `BeatHistogram`, `Tempo`, `BeatStatistics2`, `PulseClarity`, `BeatTrack3`, `AutoTrack2`, `OnsetStatistics2`, `OnsetRate`, `TempoStability`, `TempoRubato`, `BeatLoudness`, `BeatBandRatio`, `RhythmTransform`, `RhythmPattern`.
- **Spectral descriptors**: `FFTCrest2`, `FFTSpread2`, `FFTSlope2`, `FFTPower2`, `SpecEntropy`, `SpecDecrease`, `SpecComplexity`, `SpecStrongPeak`, `SpecHFC`, `SpecBandwidth`, `SpecIrregularity`, `SpecVariation`, `SpecRMS`, `SpecEnergy`, `SpecBandEnergy`, `SpecBandRatio`, `SpecSparsity`, `FFTPeak2`, `FFTSubbandFlatness2`, `FFTSubbandFlux2`, `FFTSubbandPower2`, `SpecContrast`.
- **Stereo analysis**: `StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase`.
- **Temporal descriptors**: `TemporalCentroid`, `EnvelopeSpread`, `EnvelopeSkewness`, `EnvelopeKurtosis`, `EffectiveDuration`, `DynamicComplexity`, `StrongDecay`, `TemporalDecrease`, `SilenceRate`, `FadeDetection`, `AttackTime`, `AttackSlope2`, `DecayTime`, `LogAttackTime`.
- **Tonal analysis**: `HPCP`, `Chromagram2`, `TuningFrequency`, `KeyTrack2`, `KeyClarity2`, `KeyMode2`, `Tonnetz`, `HPCPEntropy`, `HPCPCrest`, `ChromaCENS`, `ChordTrack`, `ChordStrength`, `HarmonicChange`, `ChordStats`.
- **Wavelets**: `Wavelets2`, `WaveletBands`, `WaveletEntropy`, `WaveletFlux`, `WaveletCentroid`, `WaveletSpread`, `WaveletDaub2`.

The installed `Guides/MIR2` summarizes each class. Every class has an audible
example using its measured features to control sound, as well as equations,
units, timing, source references, and limits. Most output control-rate features;
audio-input analyzers consume the complete audio block internally. FFT analyzers
read the shared FFT buffer without changing its representation.

### Musical Documentation, Audio Corpus and Live MIR Visualization

Following an overhaul directive (detailed in [MIR2_DOCUMENTATION_AND_VISUALIZATION_PLAN.md](MIR2_DOCUMENTATION_AND_VISUALIZATION_PLAN.md)),
MIR2 examples are moving beyond speech-only playback (`ExampleFiles.child`). The
documentation uses a dedicated bundle of dry, uncompressed Creative Commons Zero (CC0)
recordings in `sounds/` (`drums.wav`, `piano-chords.wav`, `melody.wav`, `plucks.wav`,
`stereo.wav`, `bass-melody-mix.wav`, `texture.wav`) accompanied by self-contained
synthetic fallbacks and commented `SoundIn.ar(0)` live inputs.

Additionally, a lightweight pure-SuperCollider live visualization toolkit (`MIR2GUI`)
provides real-time graphical monitors standard in MIR research: scrolling feature
scopes, multi-band bar graphs, 2D spectrogram heatmaps, circular chroma wheels,
2D Tonnetz harmonic grids, stereo goniometers, multi-pitch piano rolls, and rhythm radars.

Wavelets support Haar/db1–db8 and periodic or redundant symmetric boundaries.
Chord tracking offers an optional nine-quality vocabulary alongside the default
major/minor set. `ChromaCQT` provides a measured low-register resolution option
with longer analysis delay; `RhythmTransform` preserves modulation by band and
`RhythmPattern` adds an explicitly documented Bark/log-power adaptation.

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

Recompile the class library after installation and open `Guides/MIR2`.
Use matching FFT sizes and hop fractions; literal frame/band/output dimensions
bound runtime memory and computation. Check each class for startup delay,
held values, and silence behavior. Pitch confidence, voicing, periodicity and
ambiguity are distinct evidence measures.

## Implementation and evaluation

- [Onsets2 audit and comparison](docs/onset-evaluation.md): all seven classical
  methods, revised SuperFlux frontend and peak picker, and synthetic measurements.
- [Spectral and auditory definitions](docs/spectral-auditory-implementation.md):
  explicit descriptor conventions, actual time-domain gammatone filters and an
  inherited informal loudness model with stated calibration limits.
- [Temporal, stereo and feature dynamics](docs/temporal-stereo-implementation.md).
- [Wavelets and constant-Q](docs/wavelet-constantq-implementation.md).
- [ChromaCQT comparison and latency tradeoffs](docs/chroma-cqt-evaluation.md).
- [PNCC equations and independent oracle](docs/pncc-implementation.md).
- [Tonal and harmonic analysis](docs/tonal-harmonic-implementation.md).
- [Rhythm conventions and verification](docs/rhythm-implementation.md).
- [Multiband rhythm modulation](docs/rhythm-modulation-implementation.md).
- [Neural pitch source and deployment audit](docs/neural-pitch-research.md).
- [Native PESTO implementation and verification](docs/neural-pitch-implementation.md).
- [Origins](ORIGINS.md) and [verification history](docs/implementation-log.md).

No numeric equivalence with other MIR libraries or general accuracy on real
recordings is implied. Advanced pitch includes causal adaptations of published
methods; class help identifies their differences. Loudness2 is an informal MIR
control feature and does not claim ISO conformance. See [CI notes](docs/CI.md)
for the standalone build workflow; hosted CI has not been run for this expansion.

## License

GPL-3.0-or-later. [Third-party notices](THIRD_PARTY_NOTICES.md) identify the
SuperCollider loudness contour table retained under compatible GPL terms.
MIR2 embeds no Essentia/librosa runtime, model weights, or datasets. The optional
neural source adaptation has [separate notices](neural/THIRD_PARTY_NOTICES.md).
