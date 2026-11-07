![MIRa Banner](Dashboard%20MIRa%20com%20cubo%20SC%20Retro.png)

# MIRa (MIRa) — Music Information Retrieval for SuperCollider

**MIRa** is a native SuperCollider plugin library providing **139 public Music Information Retrieval (MIR) analyzer classes**, an interactive Qt live visualization toolkit (`MIRaGUI`), a curated CC0 audio test corpus, and an optional deep neural pitch tracker (`PitchPESTO`).

---

## The Concept: What is *MIRa*?

> In Portuguese, the word **mira** translates directly to **target, scope, sight, crosshair, or aim** (derived from the verb *mirar*—to gaze attentively, to aim with purpose).
>
> In real-time computer music and interactive sound design, this Portuguese meaning captures exactly what MIR UGens do analytically: **they act as an analytical crosshair aimed directly at an audio stream.**
>
> Instead of treating audio as an undifferentiated stream of oscillating samples, MIRa places an analytical sight (*mira*) onto specific acoustic, perceptual, and structural dimensions of the sound:
> - **Focusing on Pitch & Harmony**: Locking onto the fundamental frequency, tracking chord progressions, tracing harmonic centroids, and mapping 6D Tonnetz space.
> - **Targeting Rhythm & Pulse**: Calculating tempo confidence, detecting onsets, isolating meter subdivisions, and evaluating rhythmic clarity.
> - **Measuring Timbre & Texture**: Sighting spectral centroids, Shannon entropy, flatness, brightness, inharmonicity, and multi-band perceptual loudness.
> - **Dissecting Transients & Dynamics**: Aiming at attack slopes, decay envelopes, crest factors, and rolling loudness dynamics.
> - **Resolving Scales with Wavelets**: Decomposing audio into dyadic time-frequency scales via orthonormal Daubechies wavelets.
>
> By placing this analytical *mira* on sound, composers and sound designers can create responsive, feature-guided synthesis, autonomous listening agents, adaptive DSP processing, and real-time audio visualization with musical precision.

---

## Feature Overview

- **139 Public Analyzer Classes**: Spanning 12 functional MIR families covering spectral, tonal, temporal, rhythm, harmonic, auditory, stereo, wavelet, and neural analysis.
- **Pure-SuperCollider Live Visualizer Toolkit (`MIRaGUI`)**: 10 real-time Qt GUI views designed after standard MIR research tools (Chroma Wheels, 2D Tonnetz grids, Waterfall Spectrogram Heatmaps, Multi-pitch Piano Rolls, Rhythm Radars, Goniometer Stereo Scopes, and Tristimulus triangles).
- **Curated CC0 Audio Test Corpus**: 7 dry 44.1 kHz WAV recordings (`drums.wav`, `piano-chords.wav`, `melody.wav`, `plucks.wav`, `stereo.wav`, `bass-melody-mix.wav`, `texture.wav`) accompanied by self-contained synthetic fallbacks.
- **Individually Handcrafted Musical Examples**: Every single help file includes a musically meaningful synthesis or filtering process demonstrating how to steer audio with measured features, plus zero-warning SCDoc documentation.
- **Dual Server Architecture**: Native support for both standard `scsynth` and multi-threaded `supernova`.
- **Optional Neural Pitch Tracker (`PitchPESTO`)**: Deep convolutional neural pitch inference using PESTO v2, running entirely in native C++ CPU code without external Python dependencies.

---

## Included Analyzer Families (139 public analyzers)

| Family | Count | UGens Included |
| :--- | :---: | :--- |
| ESP | 4 | FreqToVolt, Periodicity2, EnvFollow2, TriggerDetector |
| **Core Analysis** | 11 | `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`, `Onsets2`, `PitchYIN`, `PitchMPM`, `Periodicity`, `Voicing`, `Pitch2` |
| **Auditory Filters & Cepstra** | 7 | `BarkBands`, `ERBBands`, `BFCC`, `LFCC`, `GammatoneBands`, `GFCC`, `Loudness2` |
| **Constant-Q Analysis** | 4 | `CQT`, `VQT`, `CQCC`, `ChromaCQT` |
| **Feature Dynamics** | 4 | `FeatureDelta`, `FeatureDeltaDelta`, `FeatureMedian`, `FeatureSmooth` |
| **Harmonic Analysis** | 11 | `HarmonicPeaks`, `Inharmonicity`, `Tristimulus`, `OddEvenRatio`, `HarmonicEnergyRatio`, `HarmonicCentroid`, `HarmonicDeviation`, `HarmonicSpread`, `HarmonicVariation`, `Noisiness`, `SensoryDissonance2` |
| **Onset Evidence** | 4 | `FFTComplexDev2`, `FFTPhaseDev2`, `FFTMKL2`, `FFTFluxPos2` |
| **Power-Normalized Cepstra** | 1 | `PNCC` |
| **Advanced Pitch** | 13 | `FundamentalCandidates`, `PitchProb`, `PitchAmbiguity`, `PitchSWIPE`, `PitchConsensus`, `PitchSalienceBuf`, `PitchSalience`, `MultiPitch`, `PredominantPitch`, `PitchContour`, `PitchStability`, `PitchDeviation`, `Vibrato2` |
| **Rhythm Analysis** | 16 | `Tempogram`, `FourierTempogram`, `BeatHistogram`, `Tempo`, `BeatStatistics2`, `PulseClarity`, `BeatTrack3`, `AutoTrack2`, `OnsetStatistics2`, `OnsetRate`, `TempoStability`, `TempoRubato`, `BeatLoudness`, `BeatBandRatio`, `RhythmTransform`, `RhythmPattern` |
| **Spectral Descriptors** | 22 | `FFTCrest2`, `FFTSpread2`, `FFTSlope2`, `FFTPower2`, `SpecEntropy`, `SpecDecrease`, `SpecComplexity`, `SpecStrongPeak`, `SpecHFC`, `SpecBandwidth`, `SpecIrregularity`, `SpecVariation`, `SpecRMS`, `SpecEnergy`, `SpecBandEnergy`, `SpecBandRatio`, `SpecSparsity`, `FFTPeak2`, `FFTSubbandFlatness2`, `FFTSubbandFlux2`, `FFTSubbandPower2`, `SpecContrast` |
| **Stereo Analysis** | 7 | `StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase` |
| **Temporal Descriptors** | 14 | `TemporalCentroid`, `EnvelopeSpread`, `EnvelopeSkewness`, `EnvelopeKurtosis`, `EffectiveDuration`, `DynamicComplexity`, `StrongDecay`, `TemporalDecrease`, `SilenceRate`, `FadeDetection`, `AttackTime`, `AttackSlope2`, `DecayTime`, `LogAttackTime` |
| **Tonal Analysis** | 14 | `HPCP`, `Chromagram2`, `TuningFrequency`, `KeyTrack2`, `KeyClarity2`, `KeyMode2`, `Tonnetz`, `HPCPEntropy`, `HPCPCrest`, `ChromaCENS`, `ChordTrack`, `ChordStrength`, `HarmonicChange`, `ChordStats` |
| **Wavelets** | 7 | `Wavelets2`, `WaveletBands`, `WaveletEntropy`, `WaveletFlux`, `WaveletCentroid`, `WaveletSpread`, `WaveletDaub2` |
| **Neural Extension** | 1 | `PitchPESTO` (Optional separate build in `neural/`) |

---

## Live MIR Visualization Toolkit (`MIRaGUI`)

MIRa includes 10 real-time GUI monitors accessible through clean static factory methods:

```supercollider
// Oscilloscope feature strip with min/max grid and units
MIRaGUI.scope("Spectral Centroid", ~bus, 100, 8000, "Hz");

// Multi-band bar view with peak hold decay
MIRaGUI.bars("MFCC Coefficients", ~bus, 13, -20.0, 20.0);

// Circular 12-pitch-class polar radar
MIRaGUI.chroma("Harmonic Pitch Profile", ~bus);

// 2D planar projection of 6D Tonnetz space
MIRaGUI.tonnetz("Tonal Centroid Space", ~bus);

// Real-time scrolling fundamental frequency piano roll
MIRaGUI.pianoRoll("PESTO Neural Pitch", ~bus, minMIDI: 48, maxMIDI: 84);

// Circular tempo and beat phase dial
MIRaGUI.rhythm("Beat Phase & Tempo", ~bus);

// Barycentric ternary harmonic distribution (T1, T2, T3)
MIRaGUI.tristimulus("Harmonic Tristimulus", ~bus);

// Polar goniometer (Lissajous X/Y phase scope) and correlation meter
MIRaGUI.stereo("Stereo Goniometer", ~bus);

// Scrolling 2D spectrogram / tempogram waterfall
MIRaGUI.heatmap("Spectrogram Waterfall", ~bus, numBins: 84);

// Triggered transient envelope and attack/decay time-domain scope
MIRaGUI.envelope("Transient Envelope", ~bus);
```

---

## Installation

### Pre-Built Binaries
1. Download the release archive for your platform from the [Releases](https://github.com/fmiramar/MIRa/releases) page.
2. Unzip and copy the `MIRa` folder into your SuperCollider `Extensions` directory:
   - **macOS**: `~/Library/Application Support/SuperCollider/Extensions/`
   - **Linux**: `~/.local/share/SuperCollider/Extensions/`
   - **Windows**: `%LOCALAPPDATA%\SuperCollider\Extensions\`
3. Restart SuperCollider or recompile the class library (`Cmd+Shift+L` or `Ctrl+Shift+L`).
4. Open the master guide: `HelpBrowser.openHelpFor("Guides/MIRa")`.

### Building from Source

Requirements: CMake 3.16+, C++17 compiler, and SuperCollider 3.14+ source or SDK.

```sh
git clone https://github.com/fmiramar/MIRa.git
cd MIRa

cmake -B build \
  -DSC_PATH=/path/to/supercollider \
  -DCMAKE_BUILD_TYPE=Release \
  -DSCSYNTH=ON -DSUPERNOVA=ON -DSTRICT=ON

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --prefix ~/Library/Application\ Support/SuperCollider/Extensions
```

---

## Verification and Quality Assurance

MIRa is thoroughly tested through an automated verification suite:
- **C++ Unit Tests**: 16 native test suites covering filter banks, CQT kernels, wavelet coefficients, PNCC noise suppression, and Tonnetz matrices (`ctest`).
- **ASan & UBSan**: Memory sanitization checks ensuring zero leaks, memory corruption, or undefined behavior.
- **Class Compilation**: Complete class tree validation (`tests/sc/compile_check.scd`).
- **Help File Syntax**: Headless SCDoc verification ensuring all 135 class help files render without warnings (`tests/sc/verify_scdoc.scd`).
- **Non-Real-Time (NRT) Audio Execution**: Headless audio synthesis test rendering all 135 help examples through `scsynth`, monitoring finite bounds and guaranteeing zero bad primitive calls (`tests/sc/generate_example_check.py --nrt`).

---

## Documentation & References

- [Master Project Guide](HelpSource/Guides/MIRa.schelp): Comprehensive overview of all 135 classes.
- [Onsets2 Evaluation](docs/onset-evaluation.md): Classical methods and SuperFlux audit.
- [Spectral & Auditory Implementation](docs/spectral-auditory-implementation.md): Math conventions and filter designs.
- [Tonal & Harmonic Analysis](docs/tonal-harmonic-implementation.md): HPCP, 108 extended chord templates, and Tonnetz.
- [Wavelet & Constant-Q Implementation](docs/wavelet-constantq-implementation.md): Dyadic scales and logarithmic frequency kernels.
- [Neural Pitch Implementation](docs/neural-pitch-implementation.md): PESTO v2 architecture and CPU inference pipeline.
- [Audio Corpus Documentation](sounds/README.md): CC0 1.0 source citations, processing commands, and provenance.

---

## License

GNU General Public License v3.0 or later (`GPL-3.0-or-later`).  
See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for details.  
The bundled audio recordings in `sounds/` are dedicated to the Public Domain under Creative Commons Zero 1.0 Universal (`CC0 1.0`).

## MS-20-inspired ESP analyzers

The documented catalog now contains 137 UGen subclasses plus two convenience analyzer wrappers (139 public analyzers). Four new normalized analyzers are available: `FreqToVolt` (`\digital` / `\circuit`), independent crossing-timing `Periodicity2`, `EnvFollow2`, and `TriggerDetector`. Every new outlet is finite in `[0,1]`. Circuit mode uses documented reduced timing and calibration assumptions; measured hardware fidelity is unverified. Existing `Periodicity` and sc3-plugins `EnvFollow` retain their behavior. See the [ESP guide](HelpSource/Guides/MIRa-MS20-ESP.schelp) and [validation record](docs/ms20-esp/validation.md).
