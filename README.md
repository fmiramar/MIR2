# MIR2

MIR2 is a native SuperCollider library for music information retrieval and
analysis-driven synthesis control. It belongs in Jumble SCience because its
algorithms implement published research techniques rather than wrap one upstream
library. It complements FluCoMa's corpus and data workflows.

This is an unpublished development expansion of version 0.1. The broader roadmap
is still in progress; see [current status](docs/roadmap-status.md).

## Included analyzers

- **Core**: `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`, `Onsets2`, `PitchYIN`, `PitchMPM`, `Periodicity`, `Voicing`, `Pitch2`.
- **Auditory**: `BarkBands`, `ERBBands`, `BFCC`, `LFCC`, `GammatoneBands`, `GFCC`, `Loudness2`.
- **FeatureDynamics**: `FeatureDelta`, `FeatureDeltaDelta`, `FeatureMedian`, `FeatureSmooth`.
- **OnsetFeatures**: `FFTComplexDev2`, `FFTPhaseDev2`, `FFTMKL2`, `FFTFluxPos2`.
- **PitchAdvanced**: `FundamentalCandidates`, `PitchProb`, `PitchAmbiguity`, `PitchSWIPE`, `PitchConsensus`, `PitchSalienceBuf`, `PitchSalience`, `MultiPitch`, `PredominantPitch`, `PitchContour`, `PitchStability`, `PitchDeviation`, `Vibrato2`.
- **SpectralAdvanced**: `FFTCrest2`, `FFTSpread2`, `FFTSlope2`, `FFTPower2`, `SpecEntropy`, `SpecDecrease`, `SpecComplexity`, `SpecStrongPeak`, `SpecHFC`, `SpecBandwidth`, `SpecIrregularity`, `SpecVariation`, `SpecRMS`, `SpecEnergy`, `SpecBandEnergy`, `SpecBandRatio`, `SpecSparsity`, `FFTPeak2`, `FFTSubbandFlatness2`, `FFTSubbandFlux2`, `FFTSubbandPower2`, `SpecContrast`.
- **Stereo**: `StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase`.
- **Temporal**: `TemporalCentroid`, `EnvelopeSpread`, `EnvelopeSkewness`, `EnvelopeKurtosis`, `EffectiveDuration`, `DynamicComplexity`, `StrongDecay`, `TemporalDecrease`, `SilenceRate`, `FadeDetection`, `AttackTime`, `AttackSlope2`, `DecayTime`, `LogAttackTime`.

The installed `Guides/MIR2` summarizes each class. Every class has an audible
example using its measured features to control sound, as well as equations,
units, timing, source references, and limits. Most output control-rate features;
audio-input analyzers consume the complete audio block internally. FFT analyzers
read the shared FFT buffer without changing its representation.

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
- [Origins](ORIGINS.md) and [verification history](docs/implementation-log.md).

No numeric equivalence with other MIR libraries or general accuracy on real
recordings is implied. Advanced pitch includes causal adaptations of published
methods; class help identifies their differences. Loudness2 is an informal MIR
control feature and does not claim ISO conformance. See [CI notes](docs/CI.md)
for the standalone build workflow; hosted CI has not been run for this expansion.

## License

GPL-3.0-or-later. [Third-party notices](THIRD_PARTY_NOTICES.md) identify the
SuperCollider loudness contour table retained under compatible GPL terms.
MIR2 embeds no Essentia/librosa runtime, model weights, or datasets.
