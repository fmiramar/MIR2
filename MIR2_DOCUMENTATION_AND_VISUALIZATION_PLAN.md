# MIR2 Documentation, Musical Examples, and Live MIR Visualization Plan

> **Author**: Antigravity Assistant (Gemini 3.8 Flash / Pair Programming Session)  
> **Date**: October 2026  
> **Scope**: `03-jumble-science/MIR2` (SuperCollider native MIR analysis library)  
> **Status**: Approved Architecture & Implementation Directive  

---

## 1. Executive Summary & Session Trajectory

### 1.1 Context and Prior Session Trajectory
During the documentation audit of the 135 base analyzers in MIR2 (plus `PitchPESTO`), the existing help file examples were found to be uniform and musically sterile: virtually every example used identical gain-scaling or simple resonant filtering on an audio clip. Furthermore, the repository-wide `AGENTS.md` guideline previously mandated using `ExampleFiles.child` (`a11wlk01.wav`, the Apollo 11 astronaut speech clip) as the default audio source. While appropriate for basic speech or speech-like tests, analyzing astronaut speech with chord estimators (`ChordTrack`), multi-pitch analyzers (`MultiPitch`), tonality engines (`KeyTrack2`), beat trackers (`BeatTrack3`), or stereo analyzers (`StereoWidth`) produces musically nonsensical results and obscures the actual functionality of the UGens.

When this tension was identified in the prior session, two strategic decisions were aligned with the user:
1. **Source Material Directive Change**: Replace the monolithic speech clip in MIR2 help examples with a curated bundle of dry, clean, loopable **Creative Commons Zero (CC0)** recordings (`drums.wav`, `piano-chords.wav`, `melody.wav`, `plucks.wav`, `stereo.wav`, `bass-melody-mix.wav`, `texture.wav`), housed in `03-jumble-science/MIR2/sounds/`, complete with provenance and license documentation. Provide self-contained synthetic fallbacks for headless testing.
2. **Standard Live MIR Visualization Toolkit (`MIR2GUI`)**: Rather than pasting repetitive, hundred-line raw GUI scripts into 135 separate `.schelp` files, implement a lightweight, reusable SuperCollider Qt GUI toolkit in `Classes/MIR2GUI.sc`. This toolkit provides standard MIR visualizations (scrolling feature scopes, multi-band bar monitors, 2D spectrogram heatmaps, circular chroma wheels, 2D Tonnetz grids, stereo goniometers, multi-pitch piano rolls, rhythm radars, and tristimulus triangles).

This document serves as the master specification, execution blueprint, and reference catalog for all 135 UGens.

---

## 2. Updated Directives and Project Rules

### 2.1 Directive Change: Audio Corpus for MIR2
- **Old Directive (`AGENTS.md`)**: "When an example could be demonstrated with live input, keep the runnable default source as `PlayBuf` looping `ExampleFiles.child`..."
- **New MIR2-Specific Directive**:
  1. MIR2 examples shall use musically appropriate, dry, uncompressed CC0 audio files located in `03-jumble-science/MIR2/sounds/`.
  2. Each help file shall first check for the presence of the sound file via `File.exists`. If absent (e.g. running in an isolated CI container or minimal install), it will fall back to an internal synthetic probe (e.g., a synthesized chord progression, drum synth, or resonant chirp) or `ExampleFiles.child`.
  3. The alternative live input `SoundIn.ar(0)` must always remain documented as a comment near the source assignment.
  4. All downloaded sound files must be strictly CC0 1.0 Universal (Public Domain Dedication) with full provenance recorded in `sounds/README.md`.

### 2.2 Subsystem Isolation Rule
- Under no circumstances may any files outside `03-jumble-science/MIR2/` be touched, edited, or deleted. In particular, `03-jumble-science/SCPhysicalModels` is actively operated by a concurrent agent.
- All Git operations must explicitly specify files within `03-jumble-science/MIR2/` (e.g. `git add 03-jumble-science/MIR2/...`).

---

## 3. Curated CC0 Audio Corpus Specification

The audio bundle lives in `03-jumble-science/MIR2/sounds/` with total size kept under 12 MB. All audio files are standardized to **44,100 Hz, 16-bit WAV, peak normalized to -1.0 dBFS**, with 5 ms zero-crossing edge fades to prevent loop boundary clicks.

| File | Target Duration | Channels | Musical Content & Attributes | Primary UGen Target Families |
| :--- | :--- | :--- | :--- | :--- |
| `drums.wav` | 8.0 s (4 bars @ 120 BPM) | Mono | Dry acoustic drum kit groove: clean kick, snappy snare, steady 8th-note hi-hat, occasional ghost notes. | Rhythm analysis (`BeatTrack3`, `Tempo`, `Tempogram`, `RhythmPattern`), Onset features (`Onsets2`, `FFTMKL2`), Temporal descriptors (`DynamicComplexity`). |
| `piano-chords.wav` | 8.0 s (4 bars @ 60 BPM or 2 bars @ 120 BPM) | Mono | Clean dry piano playing a clear diatonic cadence: Cmaj7 - Am7 - Dm7 - G7 (or I-vi-IV-V). Sustained voice-leading with clear harmonic transitions. | Tonal analysis (`HPCP`, `Chromagram2`, `KeyTrack2`, `ChordTrack`, `Tonnetz`, `ChromaCENS`), Constant-Q (`ChromaCQT`). |
| `melody.wav` | 10.0–14.0 s | Mono | Dry monophonic acoustic solo (cello, violin, or flute) playing an expressive phrase with intentional vibrato, legato transitions, and pitch variation across ~2 octaves. | Pitch analyzers (`PitchYIN`, `PitchMPM`, `Pitch2`, `PitchSWIPE`, `PitchContour`, `Vibrato2`), Harmonic analysis (`HarmonicPeaks`, `Inharmonicity`, `Tristimulus`). |
| `plucks.wav` | 8.0–12.0 s | Mono | 4–6 isolated struck/plucked notes (kalimba, glockenspiel, or classical guitar) with sharp attacks and natural ring-down separated by 1.5 s of clean silence. | Temporal descriptors (`AttackTime`, `AttackSlope2`, `LogAttackTime`, `DecayTime`, `EffectiveDuration`), Inharmonicity (`Inharmonicity`, `SensoryDissonance2`). |
| `stereo.wav` | 10.0–14.0 s | Stereo (2 ch) | Stereo mix with dynamic panning: drum kit with wide overheads, an instrument moving left-to-right, and wide out-of-phase ambient tails. | Stereo analysis (`StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase`). |
| `bass-melody-mix.wav` | 8.0–10.0 s | Mono | Polyphonic layer containing a low walking bassline and an upper melodic lead. | Advanced Pitch (`MultiPitch`, `PredominantPitch`, `FundamentalCandidates`, `PitchProb`, `PitchAmbiguity`). |
| `texture.wav` | 8.0 s | Mono | Textured noise source: bowed cymbal scraping, dry breath, or brushed snare roll with continuous spectral density. | Spectral descriptors (`SpecFlatness2`, `SpecEntropy`, `SpecSparsity`, `Noisiness`, `SilenceRate`). |

### 3.1 Provenance Documentation (`sounds/README.md`)
Each sound file must be documented in `sounds/README.md` with:
- Source title and Freesound / Wikimedia Commons URL.
- Original author / uploader name and handle.
- Explicit license confirmation (CC0 1.0 Universal).
- Description of original recording and DSP processing applied (cropping, trimming to exact bar length, resampling to 44.1 kHz, channel mixdown, peak normalization, edge fades).

---

## 4. Reusable Live MIR Visualization GUI Kit (`Classes/MIR2GUI.sc`)

### 4.1 Design Objectives
1. **Standard MIR Visual Representations**: Implement the exact visualizations used in standard MIR software (Sonic Visualiser, Essentia, Librosa, AudioSculpt, Mirex).
2. **Minimal Overhead**: Use Qt GUI primitives (`UserView`, `Pen`, `ScopeView`) updating at 25–30 Hz via a `SkipJack` polling control buses or receiving `SendReply.kr` OSC messages, ensuring zero audio dropouts.
3. **Headless Test Immunity**: Verification scripts (`tests/sc/generate_example_check.py`, `compile_help_examples.scd`) run in headless environments. The GUI invocation will be cleanly structured so the audio synthesis runs unconditionally, while the GUI window only initializes if a GUI server is active (`GUI.current.notNil`), or is provided as a companion interactive block.

### 4.2 Toolkit Components (`MIR2GUI`)

```
               ┌────────────────────────────────────────────────────────┐
               │                        MIR2GUI                         │
               └────────────────────────────────────────────────────────┘
                                           │
         ┌───────────────────┬─────────────┴───────┬────────────────────┐
         ▼                   ▼                     ▼                    ▼
  [Time-Series]       [Multiband/Bins]      [Time-Frequency]     [Specialized MIR]
  • MIR2Scope         • MIR2BarView         • MIR2Heatmap        • MIR2ChromaWheel
  • MIR2EnvelopeView  • MIR2TristimulusView • MIR2PianoRoll      • MIR2TonnetzView
                                                                 • MIR2StereoScope
                                                                 • MIR2RhythmWheel
```

#### 1. `MIR2Scope` — Scrolling Feature Strip
- **Purpose**: Real-time scrolling time-history of continuous scalar descriptors.
- **Parameters**: `name`, `minVal`, `maxVal`, `units`, `historyLength` (default 200 frames).
- **Visual Encoding**: Smooth line plot with min/max grid lines, zero-axis, current numeric readout, and historical trace.
- **Targets**: `SpecCentroid2`, `SpecFlatness2`, `SpecEntropy`, `PitchYIN`, `Inharmonicity`, `Loudness2`, `PitchStability`, `SilenceRate`, `DynamicComplexity`, etc.

#### 2. `MIR2BarView` — Multi-Band Spectrum / Cepstrum Monitor
- **Purpose**: Discrete bar graph monitor for vector features and filterbanks.
- **Parameters**: `name`, `numBars`, `minVal`, `maxVal`, `barLabels` (e.g. Bark band frequencies, MFCC coefficient indices).
- **Visual Encoding**: Gradient-colored vertical bars (cyan/blue) with trailing peak-hold indicators.
- **Targets**: `BarkBands`, `ERBBands`, `GammatoneBands`, `MFCC2`, `BFCC`, `LFCC`, `GFCC`, `PNCC`, `CQCC`, `HarmonicPeaks`.

#### 3. `MIR2Heatmap` — Scrolling 2D Waterfall / Spectrogram
- **Purpose**: Scrolling 2D matrix displaying time on the X-axis and frequency/lag/scale on the Y-axis.
- **Parameters**: `name`, `numBins`, `yMin`, `yMax`, `yScale` (`\linear`, `\log`, `\bpm`).
- **Visual Encoding**: Multi-row bitmap or vector strip with thermal colormap (dark navy $\rightarrow$ purple $\rightarrow$ orange $\rightarrow$ bright yellow).
- **Targets**: `CQT`, `VQT`, `Tempogram`, `FourierTempogram`, `BeatHistogram`, `Wavelets2`, `WaveletBands`, `PitchSalienceBuf`, `RhythmPattern`, `RhythmTransform`.

#### 4. `MIR2ChromaWheel` — Circular Pitch-Class Radar
- **Purpose**: Polar representation of the 12 chromatic pitch classes ($C, C\sharp, D \dots B$).
- **Parameters**: `tuning` (default 440 Hz).
- **Visual Encoding**: Circular radar divided into 12 labeled segments. Segment radius and color saturation indicate the energy of each pitch class.
- **Targets**: `HPCP`, `Chromagram2`, `ChromaCQT`, `ChromaCENS`.

#### 5. `MIR2TonnetzView` — 2D Projection of 6D Tonnetz Space
- **Purpose**: Visualizes harmonic motion across the three harmonic circles defined in MIR (Fifth circle, Minor Third circle, Major Third circle).
- **Visual Encoding**: Planar diagram showing three concentric or interconnected rings representing harmonic fifths and thirds. A live trajectory point (bead) moves smoothly as chords change, illustrating harmonic stability and functional key relationships.
- **Targets**: `Tonnetz`, `ChordTrack`, `KeyTrack2`, `HarmonicChange`.

#### 6. `MIR2StereoScope` — Polar Goniometer & Correlation Meter
- **Purpose**: Real-time vector scope displaying stereo soundstage distribution and interchannel phase.
- **Visual Encoding**:
  - Center: X/Y Lissajous scatter plot displaying instantaneous Left vs. Right displacement.
  - Left meter: Stereo balance $(-1 \dots +1)$ and panning position.
  - Right meter: Interchannel correlation coefficient $(-1 \dots +1)$ and stereo width ring.
- **Targets**: `StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase`.

#### 7. `MIR2PianoRoll` — Continuous Pitch & Multi-Pitch Tracker
- **Purpose**: Real-time scrolling horizontal piano roll over MIDI notes (MIDI 24–96).
- **Visual Encoding**: Horizontal keyboard grid with background semitone stripes. Detected F0 estimates are plotted as streaming lines or luminous beads; multi-pitch candidates are displayed with point size or opacity proportional to their salience.
- **Targets**: `PitchYIN`, `PitchMPM`, `Pitch2`, `PitchSWIPE`, `MultiPitch`, `PredominantPitch`, `FundamentalCandidates`, `PitchProb`, `PitchContour`.

#### 8. `MIR2RhythmWheel` — Tempo Dial & Beat Radar
- **Purpose**: Live visualization of rhythm, tempo, and pulse clarity.
- **Visual Encoding**: Circular speedometer/clock dial. Center displays numeric BPM and confidence. A rotating radar needle sweeps synchronously with beat phase; peripheral LED dots flash on detected onsets and beat subdivisions.
- **Targets**: `BeatTrack3`, `AutoTrack2`, `Tempo`, `PulseClarity`, `BeatStatistics2`, `TempoStability`, `TempoRubato`, `BeatLoudness`, `BeatBandRatio`.

#### 9. `MIR2TristimulusView` — Barycentric Ternary Triangle
- **Purpose**: Displays the timbral distribution of harmonic energy across fundamental ($T_1$), low harmonics ($T_2$), and high harmonics ($T_3$), satisfying $T_1 + T_2 + T_3 = 1$.
- **Visual Encoding**: Equilateral triangle whose vertices represent 100% Fundamental, 100% Harmonics 2–4, and 100% Harmonics 5+. A moving point with a fading trail charts the evolving timbral coordinates.
- **Targets**: `Tristimulus`, `OddEvenRatio`, `HarmonicEnergyRatio`, `HarmonicCentroid`.

#### 10. `MIR2EnvelopeView` — Attack & Transient Oscilloscope
- **Purpose**: Visualizes transient dynamics, envelope slopes, and decay phases.
- **Visual Encoding**: Time-domain envelope buffer display triggered on attacks, overlaying measured attack time, logarithmic rise curve, linear slope vector, and decay boundary markers.
- **Targets**: `AttackTime`, `AttackSlope2`, `LogAttackTime`, `DecayTime`, `EffectiveDuration`, `StrongDecay`, `TemporalCentroid`, `TemporalDecrease`.

---

## 5. Detailed Catalog: Musical Processes & Visualizations for All 135 UGens

The following catalog specifies an individually considered, musically compelling patch process and standard MIR visualization for all 135 base analyzers across the 14 families, plus the optional neural analyzer.

### 5.1 Core Family (11 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `SpecCentroid2` | `drums.wav` / `melody.wav` | **Spectral Tilt Cross-Modulator**: Centroid modulates the cross-fade between a dry acoustic signal and a pitch-shifted resonant shimmer reverb, opening an auxiliary upper harmonic comb filter only during bright attacks. | `MIR2Scope` showing spectral centroid frequency in Hz ($0 \dots 10000$ Hz) with logarithmic frequency guide lines. |
| `SpecFlatness2` | `texture.wav` + `melody.wav` | **Noise-Gated Resynthesizer**: Flatness controls the mix between a pitch-tracked saw oscillator (when tone-like) and a multiband granular diffuser (when noise-like), dynamically transforming pitched notes into frozen vapor. | `MIR2Scope` showing flatness index ($0.0 \dots 1.0$) with color transition from green (harmonic/tonal) to red (stochastic/flat). |
| `SpecPcile2` | `drums.wav` | **Adaptive Dynamic De-Esser / Sizzle Extractor**: Extracts high-frequency energy above the 85th percentile cutoff and routes it through an envelope-follower ring modulator for rhythmic sizzle percussion. | `MIR2Scope` showing percentile frequency boundary in Hz, overlaid with reference frequency markers ($2\text{k}, 5\text{k}, 10\text{k}$). |
| `MFCC2` | `melody.wav` | **Timbral Morphing Vocoder**: 13 MFCC coefficients dynamically control the filter bank gains of an excitation noise synth, reconstructing the timbral vowel color of the acoustic instrument onto a synth carrier. | `MIR2BarView` displaying 13 coefficients with bipolar $[-50, +50]$ range and center zero-axis. |
| `FFTFlux2` | `drums.wav` | **Transient Spectral Shatterer**: High spectral flux triggers a burst of micro-delay grain clouds; during steady-state sections, the signal remains clean and focused. | `MIR2Scope` showing spectral distance flux trace with adjustable trigger threshold overlay line. |
| `Onsets2` | `drums.wav` | **Tempo-Synced Granular Slicer**: Onsets trigger sample-accurate slice jumps in a secondary playback buffer with random pitch transposition and reverse envelopes. | `MIR2EnvelopeView` displaying the detection function trace with vertical trigger spikes and threshold curve. |
| `PitchYIN` | `melody.wav` | **Microtonal Intelligent Harmonizer**: Tracks fundamental pitch and voicing; feeds an auxiliary bank of resonators tuned to just-intonation intervals above the detected note, muting gracefully on unvoiced noise. | `MIR2PianoRoll` showing MIDI pitch line ($36 \dots 84$), accompanied by a horizontal confidence bar ($0 \dots 1$). |
| `PitchMPM` | `melody.wav` | **Sub-Harmonic Bass Doubler**: NSDF pitch tracking drives a heavy analog-style sub-bass oscillator with wavefolding, tracking fast pitch slides cleanly. | `MIR2PianoRoll` displaying F0 track with NSDF clarity metric mapped to line thickness. |
| `Periodicity` | `texture.wav` + `melody.wav` | **Feedback Loop Stabilizer**: Periodicity controls the damping coefficient of a self-oscillating Karplus-Strong waveguide; when the input is periodic, the waveguide resonates in tune. | `MIR2Scope` displaying periodicity metric ($0.0 \dots 1.0$). |
| `Voicing` | `melody.wav` | **Dual-Path Signal Router**: Automatically separates voiced sustains (routed into an expressive chorus/reverb) from unvoiced consonants and breath noise (routed into a gritty distortion/crusher). | `MIR2Scope` showing continuous voicing probability and discrete voiced/unvoiced gate indicator. |
| `Pitch2` | `melody.wav` | **Adaptive Auto-Wah Ring Modulator**: Modern pitch facade extracts fundamental frequency to tune an asymmetric diode ring modulator, producing consonant ring modulation intervals. | `MIR2PianoRoll` showing estimated pitch along with octave-ambiguity indicators. |

---

### 5.2 Auditory Filters and Cepstra (7 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `BarkBands` | `drums.wav` | **Critical-Band Graphic Exciter**: 24-band Bark filterbank drives individual nonlinear tape saturators per band, adding warm harmonic warmth concentrated in mid critical bands. | `MIR2BarView` with 24 Bark band bars ($1 \dots 24$) and frequency labels. |
| `ERBBands` | `drums.wav` | **Auditory-Masking Noise Shaper**: Uses Equivalent Rectangular Bandwidth energies to calculate perceptual masking curves, gating a background rain texture so it only peeks through unmasked spectral bands. | `MIR2BarView` with 40 ERB frequency bins across the auditory range. |
| `BFCC` | `melody.wav` | **Formant-Tracking Vocal Filter**: Bark-frequency cepstra drive a dynamic multi-peak formant filter bank on a sawtooth pad, cloning the vocalic contour of the instrument. | `MIR2BarView` displaying 13 BFCC coefficients with dynamic scaling. |
| `LFCC` | `melody.wav` | **Linear Spectral Resonator**: Linear cepstra drive an array of modal resonators, yielding a bell-like metallic acoustic reflection of the incoming sound. | `MIR2BarView` displaying LFCC coefficient bars. |
| `GammatoneBands` | `melody.wav` | **Cochlear Additive Resynthesizer**: Filterbank powers individual sine oscillators at cochlear center frequencies, creating an additive auditory ghost replica of the input. | `MIR2BarView` showing Gammatone band energy distribution with smooth temporal response. |
| `GFCC` | `melody.wav` | **Biomimetic Timbre Warper**: Gammatone cepstral coefficients drive a non-linear polynomial wavefolder, producing rich timbral shifts synced to performance articulation. | `MIR2BarView` showing GFCC coefficients with bipolar meter. |
| `Loudness2` | `drums.wav` | **Perceptual Dynamic Compressor**: Perceptually weighted sone measurement controls a feed-forward opto-style compressor, maintaining consistent psychoacoustic loudness without pumping. | `MIR2Scope` showing Loudness in sones, with threshold and gain-reduction trace. |

---

### 5.3 Constant-Q Analysis (4 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `CQT` | `piano-chords.wav` | **Log-Frequency Spectral Freezer**: 84-bin constant-Q spectrum triggers spectral peak holds; holding the loudest bins creates an evolving sustained chord organ backing. | `MIR2Heatmap` showing log-frequency waterfall spectrogram across 7 octaves ($55 \dots 7040$ Hz). |
| `VQT` | `drums.wav` + `piano-chords.wav` | **Transient-Aware Low-End Resonator**: Variable-Q with bandwidth offset provides low-latency bass resolution, tuning a physical-model bass resonator in real time. | `MIR2Heatmap` showing VQT energy matrix with heightened low-end temporal responsiveness. |
| `CQCC` | `piano-chords.wav` | **Harmonic Spectral Fingerprint Re-Synthesizer**: Constant-Q cepstra modulate a multi-carrier FM synthesis patch, producing a synthetic shadow of the chord instrument. | `MIR2BarView` displaying CQCC coefficient distribution. |
| `ChromaCQT` | `piano-chords.wav` | **Polyphonic Organ Extender**: 12-tone constant-Q chroma drives a 12-voice drawbar organ synth that plays sympathetic octave doublings conforming to the detected musical key. | `MIR2ChromaWheel` displaying 12 pitch classes with high low-register selectivity. |

---

### 5.4 Feature Dynamics (4 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `FeatureDelta` | `drums.wav` / `melody.wav` | **Velocity-Sensitive Glitch Repeater**: First derivative of spectral centroid detects sudden gestures; rapid increases trigger a micro-stutter buffer repeater. | `MIR2Scope` showing bipolar derivative trace ($\Delta$) centered around 0 with positive/negative surge indicators. |
| `FeatureDeltaDelta` | `melody.wav` | **Physical Gesture Particle Emittor**: Feature acceleration ($\Delta\Delta$) drives the emission density of a stochastic particle sound generator, adding sonic sparks at the onset of sudden musical flourishes. | `MIR2Scope` showing acceleration spikes with positive/negative impulse visualization. |
| `FeatureMedian` | `melody.wav` | **Glitch-Immune Smooth Controller**: Running median filter strips wild outlier spikes from noisy pitch tracking, driving a crystal-clear analog synth lead faithfully. | `MIR2Scope` showing dual traces: raw erratic feature (light gray) vs. median-filtered output (thick solid cyan). |
| `FeatureSmooth` | `drums.wav` | **Lagged Breath Reverb Swell**: One-pole smoothed envelope follower generates breathing reverb washes that bloom slowly behind percussive hits. | `MIR2Scope` showing instantaneous feature vs. exponential smoothed curve. |

---

### 5.5 Harmonic Analysis (11 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `HarmonicPeaks` | `melody.wav` | **Harmonic Additive Extender**: Extracts 16 exact harmonic partial frequencies and amplitudes, feeding an additive sine bank with extended spatial panning across the stereo field. | `MIR2BarView` showing 16 harmonic partial amplitudes at integer harmonic positions ($f_0, 2f_0, \dots, 16f_0$). |
| `Inharmonicity` | `plucks.wav` | **Adaptive Stiff-String Modeler**: Inharmonicity controls the dispersion factor of a physical string model, transitioning from a warm nylon guitar to a stiff, metallic bar. | `MIR2Scope` showing inharmonicity index ($0.0 \dots 1.0$) with metallic stiffness indicator. |
| `Tristimulus` | `melody.wav` | **Dynamic Timbre Color Crossfader**: 3-component harmonic energy partitions ($T_1, T_2, T_3$) dynamically crossfade three distinct synthesis textures: sub-warmth, body, and crystal air. | `MIR2TristimulusView` showing the barycentric triangle trajectory between fundamental, mid, and high partials. |
| `OddEvenRatio` | `melody.wav` | **Square-to-Sawtooth Morphing Synth**: Odd-to-even harmonic ratio controls the symmetry of a variable-shape oscillator, morphing from hollow clarinet-like tone to buzzy brass tone. | `MIR2Scope` showing odd/even ratio on logarithmic scale with 1.0 (balanced) center line. |
| `HarmonicEnergyRatio`| `melody.wav` + `texture.wav` | **Harmonic Comb Resonator Gater**: When harmonic energy ratio is high, activates a feedback comb filter; when low, switches to a diffusion network. | `MIR2Scope` showing harmonic energy ratio ($0.0 \dots 1.0$). |
| `HarmonicCentroid` | `melody.wav` | **Partial-Weighted Brightness Filter**: Harmonic centroid controls the resonance frequency of an audio-rate state-variable filter centered purely within the harmonic series. | `MIR2Scope` showing harmonic spectral centroid in Hz. |
| `HarmonicDeviation` | `melody.wav` | **Spectral Ripple Comb Generator**: Deviation from a smooth spectral envelope modulates the depth of a comb flanger, exaggerating uneven harmonic peaks. | `MIR2Scope` showing harmonic deviation metric. |
| `HarmonicSpread` | `melody.wav` | **Stereo Spectral Panner**: Harmonic spread controls the width of a frequency-dependent stereo splitter, fanning out broad harmonic spectra across the stereo image. | `MIR2Scope` showing harmonic spread in Hz. |
| `HarmonicVariation` | `melody.wav` | **Spectral Motion Trigger**: Rapid changes in the harmonic distribution trigger ambient granular clouds, accentuating expressive articulation shifts. | `MIR2Scope` showing normalized harmonic variation ($0.0 \dots 1.0$). |
| `Noisiness` | `texture.wav` + `melody.wav` | **Adaptive Noise Bleeder**: Detects energy outside harmonic partials; controls a noise gate that blends a matching bandpass-filtered pink noise generator for textured breathiness. | `MIR2Scope` showing noisiness proportion ($0.0 \dots 1.0$). |
| `SensoryDissonance2`| `piano-chords.wav` | **Roughness Distortion Driver**: Pairwise dissonance of spectral peaks drives a saturation distortion; consonant intervals remain pure and clean while dissonant clusters crunch intensely. | `MIR2Scope` showing sensory dissonance index with roughness scale markers. |

---

### 5.6 Onset Evidence (4 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `FFTComplexDev2` | `drums.wav` | **Phase-Aware Transient Gate**: Complex spectral prediction error detects subtle tonal and percussive transitions, triggering sample-accurate digital clicks. | `MIR2Scope` showing complex deviation curve with peak onset markers. |
| `FFTPhaseDev2` | `drums.wav` | **Tonal Attack Sniffer**: Wrapped second phase difference detects pitched attacks even at low amplitude, firing an arpeggiator clock. | `MIR2Scope` showing phase deviation metric. |
| `FFTMKL2` | `drums.wav` | **Modified KL Energy Slicer**: Modified Kullback-Leibler divergence emphasizes quiet high-frequency percussive attacks, driving an adaptive transient envelope shaper. | `MIR2Scope` showing MKL onset detection function. |
| `FFTFluxPos2` | `drums.wav` | **Dynamic Spectral Gate**: Half-wave rectified positive flux opens a fast VCA, allowing only the attack portion of drums to feed an external reverb tank. | `MIR2Scope` showing positive flux trace with dynamic threshold line. |

---

### 5.7 Power-Normalized Cepstra (1 UGen)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `PNCC` | `melody.wav` | **Noise-Robust Timbre Recognizer**: Power-normalized cepstral coefficients drive a vowel-formant resonator bank, tracking vocal and instrumental timbre even through noisy backgrounds. | `MIR2BarView` displaying 13 PNCC coefficient bars with power-normalized scale. |

---

### 5.8 Advanced Pitch (13 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `FundamentalCandidates` | `piano-chords.wav` / `bass-melody-mix.wav` | **Polyphonic Voice Resynthesizer**: Tracks up to 4 competing F0 candidates and their weights; synthesizes 4 independent sine voices tracking the polyphonic chord voices. | `MIR2PianoRoll` showing multiple candidate beads with radius proportional to candidate weight. |
| `PitchProb` | `melody.wav` | **Probabilistic Drone Tuner**: Multi-threshold YIN provides continuous pitch distribution; controls the master tuning of a lush ambient drone synthesizer. | `MIR2PianoRoll` with pitch uncertainty band shown as a semi-transparent confidence halo. |
| `PitchAmbiguity` | `melody.wav` + `texture.wav` | **Ambiguity-Controlled Ring Modulator**: Octave competition entropy modulates the wet/dry balance of a complex ring modulator, generating microtonal chaos on ambiguous notes. | `MIR2Scope` showing candidate entropy ($0.0 \dots 1.0$). |
| `PitchSWIPE` | `melody.wav` | **Prime-Harmonic Vocal Doubler**: Sawtooth waveform matching SWIPE algorithm tracks lead pitch; drives a formant-preserving vocal harmonizer. | `MIR2PianoRoll` showing SWIPE pitch estimate with harmonic strength metric. |
| `PitchConsensus` | `melody.wav` | **Fault-Tolerant Pitch Tracker**: Fuses multiple pitch algorithms; whenever consensus agreement is high, routes audio to a pure resonant comb; when estimators disagree, softly mutes. | `MIR2PianoRoll` displaying consensus pitch line and cross-algorithm agreement score. |
| `PitchSalienceBuf` | `melody.wav` / `piano-chords.wav` | **Visual Spectral Pitch Sculptor**: Writes a dense 60-bin-per-octave pitch-salience grid into a Buffer; buffer frames drive a multi-tap granular delay network. | `MIR2Heatmap` showing dense pitch-salience matrix over time ($55 \dots 1760$ Hz). |
| `PitchSalience` | `melody.wav` | **Instantaneous Pitch Salience Monitor**: Control-rate pitch salience drives a sub-bass sine wave whose amplitude is scaled directly by pitch clarity. | `MIR2Scope` showing pitch salience metric ($0.0 \dots 1.0$). |
| `MultiPitch` | `bass-melody-mix.wav` / `piano-chords.wav` | **Automatic Bass & Treble Splitter**: Simultaneously tracks fundamental frequencies of bass and soprano lines; synthesizes an analog sub-bass and a crystalline high chime. | `MIR2PianoRoll` displaying multiple distinct F0 trajectories with color-coded voice indices. |
| `PredominantPitch` | `bass-melody-mix.wav` | **Melody Extractor Synth**: Causal dynamic tracker extracts the leading melodic line from a dense texture, doubling the lead melody with a Moog-style lead synth. | `MIR2PianoRoll` showing the extracted single predominant melody trajectory. |
| `PitchContour` | `melody.wav` | **Gesture-Tracking Glissando Synthesizer**: Dynamic programming tracker follows smooth melodic contours, driving an expressive theremin-style sine oscillator. | `MIR2PianoRoll` showing continuous pitch contour curve. |
| `PitchStability` | `melody.wav` | **Vibrato-Triggered Delay Send**: Tracks pitch variance over a rolling window; when a note settles into steady pitch stability, it opens a feedback send to a lush stereo tape delay. | `MIR2Scope` showing pitch stability metric in cents deviation. |
| `PitchDeviation` | `melody.wav` | **Microtonal Cents Auto-Tuner**: Measures signed deviation in cents against 12-TET equal temperament; controls a real-time pitch-shifter that gently nudges the performance into tuning. | `MIR2Scope` showing bipolar cents deviation ($-50 \dots +50$ cents) with zero center tuning line. |
| `Vibrato2` | `melody.wav` | **Vibrato-Synced Tremolo LFO**: Measures vibrato speed (Hz) and depth (cents); synchronizes an amplitude tremolo LFO to match the performer's natural vibrato rate. | `MIR2Scope` showing dual meters: Vibrato Rate ($3 \dots 9$ Hz) and Vibrato Extent (cents). |

---

### 5.9 Rhythm Analysis (16 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `Tempogram` | `drums.wav` | **Rhythmic Poly-Meter Modulator**: Autocorrelation tempogram drives an array of rhythmic delay lines synced to integer fractions of detected tempo peaks. | `MIR2Heatmap` displaying autocorrelation tempogram ($40 \dots 240$ BPM lag spectrum). |
| `FourierTempogram` | `drums.wav` | **Fourier Tempo Spectrogram**: Fourier tempo analysis drives a tempo-synced chorus and phaser whose modulation rates lock to the primary cyclic tempo components. | `MIR2Heatmap` displaying Fourier tempogram spectrum. |
| `BeatHistogram` | `drums.wav` | **Metric Weight Resynthesizer**: Beat histogram bins control the gains of tuned rhythmic kick drums at metric tempo divisions (e.g. 60, 120, 180 BPM). | `MIR2BarView` showing tempo distribution histogram across BPM bins. |
| `Tempo` | `drums.wav` | **Master Clock Generator**: Causal tempo tracker drives the tempo of an algorithmic MIDI sequencer, playing synth arpeggios that follow live drum tempo changes. | `MIR2RhythmWheel` displaying live BPM readout, confidence bar, and spinning tempo wheel. |
| `BeatStatistics2` | `drums.wav` | **Groove Swing Analyzer**: First and second tempo candidates and regularity statistics modulate the swing factor of a secondary hi-hat generator. | `MIR2Scope` displaying primary vs. secondary tempo candidates and regularity ratio. |
| `PulseClarity` | `drums.wav` + `texture.wav` | **Metric Gater**: Pulse clarity controls an envelope gate on an ambient pad; when a strong groove is detected, the pad rhythmically pumps; during free-time, it washes out. | `MIR2Scope` showing pulse clarity metric ($0.0 \dots 1.0$). |
| `BeatTrack3` | `drums.wav` | **Quantized Rhythmic Glitch Looper**: Beat and subdivision triggers drive a beat-slicing delay that stutters on exact 16th-note subdivisions and resets on bar downbeats. | `MIR2RhythmWheel` with flashing quarter-note beat indicator and subdivision ring. |
| `AutoTrack2` | `drums.wav` | **Adaptive Click-Track Resynthesizer**: Kalman-filtered tempo and phase tracking triggers synthetic woodblock ticks on quarter-note downbeats. | `MIR2RhythmWheel` showing beat phase accumulation and tempo dial. |
| `OnsetStatistics2` | `drums.wav` | **Onset Density Arpeggiator**: Inter-onset interval mean and variance control the rate and note-density of an algorithmic generative bell cluster. | `MIR2Scope` displaying mean IOI (seconds) and IOI standard deviation. |
| `OnsetRate` | `drums.wav` | **Dynamic Activity Distortion**: Rolling onset rate (attacks/sec) drives the drive level of a tube amp simulator; faster playing becomes progressively more overdriven. | `MIR2Scope` showing onsets per second ($0 \dots 20$ events/s). |
| `TempoStability` | `drums.wav` | **Clock-Drift Flanger**: Stability of detected tempo controls the feedback of an analog BBD flanger; rock-solid tempos create deep resonant combs; fluctuating tempos create warble. | `MIR2Scope` showing tempo stability percentage ($0 \dots 100\%$). |
| `TempoRubato` | `drums.wav` | **Expressive Accelerando Detector**: Measures rubato accelerando/decelerando; modulates the cutoff frequency of a sweeping lowpass filter in response to expressive tempo swells. | `MIR2Scope` showing signed rubato acceleration/deceleration trace. |
| `BeatLoudness` | `drums.wav` | **Accented Beat Sidechain Compressor**: Beat loudness metric triggers an aggressive sidechain pump ducking the accompaniment on heavy downbeat drum accents. | `MIR2Scope` showing instantaneous beat loudness in sones/dB. |
| `BeatBandRatio` | `drums.wav` | **Kick-to-Snare Spectral Balancer**: Ratio of low-band to high-band rhythmic energy automatically adjusts an active tilt EQ, boosting top-end sparkle when kicks dominate. | `MIR2Scope` showing low-to-high rhythmic band ratio. |
| `RhythmTransform` | `drums.wav` | **Multiband Modulation Matrix Resynthesizer**: Per-band modulation spectrum drives a multiband rhythmic vocoder, printing complex rhythmic pulses across 8 frequency bands. | `MIR2Heatmap` showing 2D matrix of frequency bands vs. modulation frequencies ($0.5 \dots 20$ Hz). |
| `RhythmPattern` | `drums.wav` | **Rhythmic Fingerprint Morph**: 24 Bark-band rhythmic fluctuation patterns modulate the spatial panning of high and low drum elements. | `MIR2Heatmap` showing Bark bands vs. modulation tempo pattern. |

---

### 5.10 Spectral Descriptors (22 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `FFTCrest2` | `drums.wav` | **Transient Peak Exciter**: Spectral crest factor opens a parallel multiband distortion channel during peaky, sharp transients, preserving warmth in sustained passages. | `MIR2Scope` showing crest factor with peak threshold line. |
| `FFTSpread2` | `texture.wav` + `melody.wav` | **Spectral Width Panner**: Second central moment of the spectrum controls stereo widening; narrow sinusoidal tones stay centered, while broad textures spread wide. | `MIR2Scope` showing spectral spread in Hz. |
| `FFTSlope2` | `drums.wav` | **Adaptive Spectral Brightness Tilt**: Linear regression spectral slope controls the tilt frequency of an active shelving EQ, restoring high-frequency presence dynamically. | `MIR2Scope` showing spectral slope ($dB/\text{kHz}$ or linear). |
| `FFTPower2` | `drums.wav` | **Summed Spectral Energy Limiter**: True spectral power drives a high-speed brickwall safety limiter with lookahead compensation. | `MIR2Scope` showing spectral power in dBFS. |
| `SpecEntropy` | `texture.wav` + `melody.wav` | **Complexity-Driven Granular Scatter**: High spectral entropy (noise-like) routes the signal to a granular scatter delay; low entropy (pure tone) feeds a clean delay line. | `MIR2Scope` showing normalized spectral entropy ($0.0 \dots 1.0$). |
| `SpecDecrease` | `drums.wav` | **Perceptual Spectral Slope Compressor**: Spectral decrease measures relative energy fall-off from fundamental to Nyquist; drives an adaptive high-frequency saturator. | `MIR2Scope` showing spectral decrease index. |
| `SpecComplexity` | `piano-chords.wav` | **Harmonic Density Chord Trigger**: Number of prominent spectral peaks modulates the feedback gain of an ambient reverb, creating lush tails for dense chords. | `MIR2Scope` showing number of detected complex spectral peaks. |
| `SpecStrongPeak` | `melody.wav` | **Formant Peaking Follower**: Detects the sharpest spectral peak; tunes a high-Q bandpass peak filter to track and emphasize the most resonant note formant. | `MIR2Scope` showing strongest peak frequency (Hz) and magnitude. |
| `SpecHFC` | `drums.wav` | **High-Frequency Content Percussion Gate**: High-frequency content metric opens a fast noise gate on drum overheads, letting cymbals through with zero pump. | `MIR2Scope` showing HFC energy curve with threshold line. |
| `SpecBandwidth` | `melody.wav` | **Formant Bandwidth Resonator**: Spectral bandwidth controls the resonance Q of a parallel formant filter, narrowing into laser focus on pure pitches. | `MIR2Scope` showing spectral bandwidth in Hz. |
| `SpecIrregularity`| `melody.wav` | **Spectral Jaggedness Shaper**: Irregularity (amplitude variance across adjacent bins) modulates a wavefolding shaper, smoothing rough spectral valleys. | `MIR2Scope` showing spectral irregularity metric. |
| `SpecVariation` | `drums.wav` | **Spectral Distance Flux Trigger**: Frame-to-frame normalized spectral variation triggers momentary reverse tape swells on sudden sonic transitions. | `MIR2Scope` showing spectral variation metric ($0.0 \dots 1.0$). |
| `SpecRMS` | `drums.wav` | **Calibrated Spectral RMS Limiter**: Frequency-domain RMS drives a smooth mastering-grade optical compressor. | `MIR2Scope` showing spectral RMS in dB. |
| `SpecEnergy` | `drums.wav` | **Energy Envelope Follower**: Total spectral energy modulates an analog tape bias parameter, producing tape saturation on loud hits. | `MIR2Scope` showing linear/log spectral energy. |
| `SpecBandEnergy` | `drums.wav` | **Isolated Sub-Bass Extractor**: Measures exact energy within $30 \dots 120$ Hz, driving a clean synthesized sub-bass sine wave reinforcing low-end weight. | `MIR2Scope` showing band energy in user-specified Hz range. |
| `SpecBandRatio` | `drums.wav` | **Sub-to-Air Balance Controller**: Ratio of low-band ($40 \dots 200$ Hz) to high-band ($5\text{k} \dots 15\text{k}$ Hz) dynamically rebalances the mix with a dual-band VCA. | `MIR2Scope` showing band energy ratio on bipolar dB scale. |
| `SpecSparsity` | `texture.wav` + `melody.wav` | **Sparsity-Controlled Additive Resynthesizer**: When spectrum is sparse (few strong tones), activates an additive sine resynthesizer; when dense/diffuse, mutes resynthesis. | `MIR2Scope` showing Hoyer sparsity index ($0.0 \dots 1.0$). |
| `FFTPeak2` | `melody.wav` | **Dominant Peak Ring Modulator**: Tracks the frequency and magnitude of the absolute maximum FFT bin, tuning a heterodyne carrier oscillator. | `MIR2Scope` showing dominant peak frequency (Hz) and magnitude. |
| `FFTSubbandFlatness2` | `drums.wav` | **Multiband Noise Expander**: 4 subband flatness measures control independent downward expanders, removing background hiss in quiet subbands. | `MIR2BarView` showing flatness across 4 subbands. |
| `FFTSubbandFlux2` | `drums.wav` | **Multiband Transient Splitter**: Measures flux in low, mid, and high bands; routes low-flux transients to a sub-bass thump and high-flux transients to an air delay. | `MIR2BarView` showing flux across subbands. |
| `FFTSubbandPower2` | `drums.wav` | **Multiband Dynamic Equalizer**: 4 subband power outputs drive a dynamic parametric EQ compensating for frequency imbalances in real time. | `MIR2BarView` showing power across subbands. |
| `SpecContrast` | `drums.wav` + `melody.wav` | **Octave Spectral Contrast Enhancer**: Measures peak-to-valley contrast across 6 octave subbands; boosts contrast using a multiband dynamic expander. | `MIR2BarView` showing peak, valley, and contrast difference for each subband. |

---

### 5.11 Stereo Analysis (7 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `StereoBalance` | `stereo.wav` | **Auto-Centering Intelligent Panner**: Detects stereo energy imbalance and applies compensating differential gain to keep the mix center-aligned. | `MIR2StereoScope` with balance needle indicating Left/Right position ($-1.0 \dots +1.0$). |
| `StereoWidth` | `stereo.wav` | **Dynamic Mid/Side Stereo Enhancer**: Stereo width modulates the Side channel gain in a Mid/Side matrix; narrow sources are gently widened; overly wide sources are mono-tamed. | `MIR2StereoScope` showing width radius meter ($0.0 \dots 2.0$). |
| `StereoCorrelation` | `stereo.wav` | **Mono-Compatibility Phase Guard**: When interchannel correlation drops below zero (phase cancellation danger), smoothly collapses out-of-phase low bass to mono. | `MIR2StereoScope` showing correlation meter ($-1.0 \dots +1.0$) with red out-of-phase warning zone. |
| `MidSideRatio` | `stereo.wav` | **M/S Spatial Compressor**: Mid/Side power ratio controls independent compressors on Mid and Side signals, stabilizing the acoustic stereo image. | `MIR2StereoScope` showing Mid vs. Side dB meter. |
| `StereoCoherence` | `stereo.wav` | **Diffuse Soundfield Extractor**: Interchannel spectral coherence separates coherent direct sound (dry lead) from incoherent diffuse sound (ambient reverb wash). | `MIR2StereoScope` showing coherence coefficient ($0.0 \dots 1.0$). |
| `StereoPanning` | `stereo.wav` | **Soundstage Target Tracker**: Tracks the instantaneous panning angle of the dominant sound source, aiming a directional spatial delay line in that virtual direction. | `MIR2StereoScope` displaying polar angle panning ray. |
| `InterchannelPhase`| `stereo.wav` | **Stereo Phase Aligner**: Measures frequency-dependent interchannel phase difference, driving an all-pass delay network to correct phase misalignments. | `MIR2StereoScope` showing phase difference in radians ($-\pi \dots +\pi$). |

---

### 5.12 Temporal Descriptors (14 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `TemporalCentroid` | `plucks.wav` | **Transient Shape Morpher**: Temporal center-of-mass within a rolling window modulates the attack curve of an envelope generator, creating backwards swells on back-heavy sounds. | `MIR2EnvelopeView` displaying temporal centroid marker within the analysis window. |
| `EnvelopeSpread` | `plucks.wav` | **Duration-Sensitive Spatial Echo**: Second central moment of energy envelope controls echo feedback; tight impulses get multiple repeats; drawn-out sustains get a single echo. | `MIR2Scope` showing envelope spread in seconds. |
| `EnvelopeSkewness` | `plucks.wav` | **Asymmetry-Driven Waveshaper**: Skewness (asymmetry between attack and decay) controls the drive and symmetry of an analog wavefolder. | `MIR2Scope` showing envelope skewness metric. |
| `EnvelopeKurtosis` | `plucks.wav` | **Impulsiveness Detector Gate**: Measures peakedness/kurtosis of the signal; impulses (high kurtosis) fire a high-frequency sparkle layer; flat envelopes leave it silent. | `MIR2Scope` showing kurtosis index with impulsive threshold line. |
| `EffectiveDuration`| `plucks.wav` | **Sustain-Length Reverb Matcher**: Measures duration during which energy remains within -10 dB of peak; sets the decay time of an algorithmic reverb to match the source note length. | `MIR2EnvelopeView` showing effective duration boundaries highlighted on envelope. |
| `DynamicComplexity`| `drums.wav` | **Complexity-Adaptive Granular Cloud**: Measures complexity and variance of amplitude envelope; high rhythmic complexity introduces granular scatter particles. | `MIR2Scope` showing dynamic complexity metric. |
| `StrongDecay` | `plucks.wav` | **Per-Note Ring-Down Enhancer**: Detects rapid drop-offs after peaks; activates a sympathetic resonant comb filter extending natural instrument sustain. | `MIR2EnvelopeView` showing decay profile with decay rate slope. |
| `TemporalDecrease` | `plucks.wav` | **Exponential Decay Linearizer**: Measures envelope decrease slope; controls a dynamic expander to linearize decay fades. | `MIR2Scope` showing temporal decrease coefficient. |
| `SilenceRate` | `drums.wav` / `texture.wav` | **Ambient Fill Generator**: Tracks percentage of silence frames; when silence rate is high (sparse playing), blends in an ambient drone to fill musical space. | `MIR2Scope` showing silence percentage ($0 \dots 100\%$). |
| `FadeDetection` | `melody.wav` / `plucks.wav` | **Fade In/Out Crossfader**: Detects whether an event is fading in or fading out; crossfades an octave-shifted shimmer pad during fade-outs. | `MIR2Scope` showing signed fade slope (positive = fade-in, negative = fade-out). |
| `AttackTime` | `plucks.wav` | **Transient Attack Shaper**: Measures physical attack duration in seconds; modulates the attack time of an auxiliary noise layer to blend seamlessly with the acoustic onset. | `MIR2EnvelopeView` displaying measured attack duration from onset to peak. |
| `AttackSlope2` | `plucks.wav` | **Percussive Punch Enhancer**: Measures linear slope of the attack transient; steep slopes (hard plucks/strikes) trigger a punchy sub-bass transient click. | `MIR2EnvelopeView` displaying attack vector slope line. |
| `DecayTime` | `plucks.wav` | **Decay-Synced Feedback Delay**: Measures time from peak to -20 dB floor; automatically tunes the delay feedback time to decay synchronously with the acoustic note. | `MIR2EnvelopeView` displaying measured decay time boundary. |
| `LogAttackTime` | `plucks.wav` | **Perceptual Attack Classifier**: Measures $\log_{10}(\text{attack time})$; classifies sounds as struck/plucked vs. bowed/blown, routing each class to tailored DSP processing chains. | `MIR2Scope` showing log attack time with instrument class zones. |

---

### 5.13 Tonal Analysis (14 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `HPCP` | `piano-chords.wav` | **Harmonic Pitch-Class Synthesizer**: Harmonic Pitch Class Profile drives a bank of 12 sine oscillators at fundamental pitch class frequencies, creating an ethereal resynthesized chroma shadow. | `MIR2ChromaWheel` displaying 12 pitch classes ($C \dots B$) with radius proportional to energy. |
| `Chromagram2` | `piano-chords.wav` | **Chroma-Driven Auto-Harmonizer**: Standard chromagram controls the resonant frequencies of 12 modal resonators excitation-driven by a plucked string. | `MIR2ChromaWheel` showing chromagram distribution across 12 pitch classes. |
| `TuningFrequency` | `piano-chords.wav` / `melody.wav` | **Master Concert Pitch Auto-Calibrator**: Estimates reference concert pitch ($430 \dots 450$ Hz); automatically shifts an external synth patch to match acoustic instruments tuned to $A=432$ Hz or $A=442$ Hz. | `MIR2Scope` showing detected tuning in Hz ($430 \dots 450$ Hz) with 440 Hz reference line. |
| `KeyTrack2` | `piano-chords.wav` | **Intelligent Modal Bass Generator**: Continuous key and scale mode tracking (Major vs. Minor) selects appropriate root notes and scale intervals for an automated bass synthesizer. | `MIR2ChromaWheel` with detected Key root note highlight and Major/Minor mode badge. |
| `KeyClarity2` | `piano-chords.wav` | **Harmonic Confidence Pad**: Key clarity score controls the cutoff of a lush pad filter; when chord changes settle into a clear tonality, the pad opens brightly. | `MIR2Scope` showing key clarity metric ($0.0 \dots 1.0$). |
| `KeyMode2` | `piano-chords.wav` | **Major/Minor Emotion Colorizer**: Detects Major (1) vs. Minor (0) mode; modulates the timbre of an ambient chord synth between warm, open major voicings and dark, filtered minor voicings. | `MIR2Scope` showing continuous mode index ($0 = \text{Minor}, 1 = \text{Major}$). |
| `Tonnetz` | `piano-chords.wav` | **Harmonic Trajectory Ambient Pad**: 6D Tonnetz coordinates (Fifths, Minor Thirds, Major Thirds) control the spatial panning and filter cutoffs of an ambient string ensemble. | `MIR2TonnetzView` displaying the 2D planar projection of harmonic motion. |
| `HPCPEntropy` | `piano-chords.wav` | **Tonal Dispersion Granulator**: Chroma entropy measures harmonic ambiguity; when chords are complex/atonal, granulates audio into a cloud; when chords are pure triads, preserves clarity. | `MIR2Scope` showing HPCP entropy ($0.0 \dots 1.0$). |
| `HPCPCrest` | `piano-chords.wav` | **Pitch Focus Resonator**: Crest factor of pitch classes drives an audio feedback loop; highly focused pitch classes resonate cleanly without oscillation. | `MIR2Scope` showing pitch class crest factor. |
| `ChromaCENS` | `piano-chords.wav` | **Long-Term Chroma Alignment Sequencer**: Energy-normalized statistics over time provide stable chroma that triggers harmonically matched audio loop stems. | `MIR2ChromaWheel` showing smooth CENS chroma contours immune to arpeggio fluctuations. |
| `ChordTrack` | `piano-chords.wav` | **Automated Accompaniment Arranger**: Detects chord roots and qualities (Major, Minor, 7th, Diminished); triggers an algorithmic backing band playing matching chord voicings. | `MIR2TonnetzView` showing current detected chord name badge (e.g. "Cmaj7", "Am") over Tonnetz. |
| `ChordStrength` | `piano-chords.wav` | **Chord Confidence VCA**: Modulates the volume of an automated chord backing track; when chord recognition confidence is low during transitions, gently ducks the backing. | `MIR2Scope` showing chord match correlation strength ($0.0 \dots 1.0$). |
| `HarmonicChange` | `piano-chords.wav` | **Chord Boundary Slice Trigger**: Detects harmonic transitions (chord changes); triggers gentle crossfades or percussion chimes exactly on new chord arrivals. | `MIR2Scope` showing harmonic change detection function with peak trigger line. |
| `ChordStats` | `piano-chords.wav` | **Harmonic Markov Generator**: Rolling chord transition statistics generate probability-weighted companion harmonies on a secondary synthesizer. | `MIR2BarView` showing probabilities across primary chord candidates. |

---

### 5.14 Wavelets (7 UGens)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `Wavelets2` | `drums.wav` | **Multiresolution Wavelet Vocoder**: 6-level discrete wavelet decomposition energy scales modulate the gains of 6 octaves of colored noise, reconstructing a percussive wavelet shadow. | `MIR2Heatmap` showing multiresolution wavelet scale decomposition across 6 octave levels. |
| `WaveletBands` | `drums.wav` | **Octave-Band Wavelet Transient Gate**: Decomposes input into wavelet frequency bands; gates each band independently to isolate kick transient, snare crack, and hi-hat sizzle. | `MIR2BarView` showing energy per wavelet scale band. |
| `WaveletEntropy` | `texture.wav` + `melody.wav` | **Multiscale Disorder Reverb Modulator**: Wavelet entropy measures disorder across time-scale planes; controls the diffusion and decay time of an FDN reverb. | `MIR2Scope` showing wavelet entropy metric. |
| `WaveletFlux` | `drums.wav` | **Multiscale Transient Sniffer**: Wavelet flux detects transients across scale levels; fires crisp FM percussion sounds synced to multi-frequency hits. | `MIR2Scope` showing wavelet flux curve with threshold markers. |
| `WaveletCentroid` | `melody.wav` | **Time-Scale Centroid Filter**: Wavelet-domain centroid tunes a state-variable lowpass filter with octave-scale tracking speed. | `MIR2Scope` showing wavelet centroid level ($1 \dots 7$). |
| `WaveletSpread` | `melody.wav` | **Wavelet Dispersion Enhancer**: Spread across wavelet scales controls the stereo widening of a multiresolution stereo chorus. | `MIR2Scope` showing wavelet dispersion spread. |
| `WaveletDaub2` | `drums.wav` | **Audio-Rate Wavelet Subband Isolator**: Reconstructs audio with selected wavelet subbands removed; strips high-frequency transient detail or isolates bass fundamentals at audio rate. | `MIR2Scope` displaying live before/after waveform scope. |

---

### 5.15 Optional Neural Extension (1 UGen)

| UGen | Recommended Sound Source | Musically Compelling Synthesis Process | Standard MIR Visualization GUI |
| :--- | :--- | :--- | :--- |
| `PitchPESTO` | `melody.wav` | **Neural Melodic Voice Cloner**: Deep convolutional pitch inference tracks monophonic pitch with exceptional noise immunity; drives a polyphonic wavetable synth tracking subtle micro-vibrato. | `MIR2PianoRoll` showing neural pitch trajectory alongside confidence probability heatmap. |

---

## 6. Implementation Roadmap and Verification Strategy

### 6.1 Phase 1: Curate and Install CC0 Audio Corpus
1. Download 7 clean, dry, verified CC0 recordings conforming to Section 3.
2. Process them with `sox` / `ffmpeg`: crop to exact musical lengths, resample to 44.1 kHz, convert to 16-bit mono WAV (stereo for `stereo.wav`), peak-normalize to -1 dBFS, and apply 5 ms edge fades.
3. Place files in `03-jumble-science/MIR2/sounds/`.
4. Create `03-jumble-science/MIR2/sounds/README.md` containing all source citations, Freesound IDs, author handles, CC0 1.0 license links, and processing commands.

### 6.2 Phase 2: Implement `Classes/MIR2GUI.sc`
1. Write `MIR2GUI.sc` containing all 10 specialized MIR visualization classes described in Section 4.
2. Provide simple, elegant factory methods (e.g. `MIR2GUI.scope(...)`, `MIR2GUI.chroma(...)`, `MIR2GUI.tonnetz(...)`, `MIR2GUI.pianoRoll(...)`).
3. Ensure all GUI views cleanly handle window closing, stop their polling `SkipJack` routines on close, and never throw errors when the server is stopped.
4. Verify compilation with `sclang tests/sc/compile_check.scd`.

### 6.3 Phase 3: Help File Overhaul (`HelpSource/Classes/*.schelp`)
For each of the 135 `.schelp` files:
1. Update the `examples::` section:
   - Provide a musically compelling synthesis/processing example as cataloged in Section 5.
   - Code snippet begins by loading the appropriate CC0 sound from `sounds/`, falling back to a clean internal synth probe or `ExampleFiles.child` if the sound file is absent.
   - Provide a companion interactive GUI snippet calling the appropriate `MIR2GUI` visualization.
   - Wrap all code blocks cleanly in `code::` and parentheses `( ... )`.
2. Ensure the class help files pass all SCDoc tag syntax rules (lowercase tags, closed blocks, clean `See also` links).

### 6.4 Phase 4: Verification and Quality Assurance
Execute the established MIR2 verification battery:
1. `sclang tests/sc/compile_check.scd` — ensures no class collisions or syntax failures.
2. `sclang tests/sc/compile_help_examples.scd` — verifies that every single example compiles cleanly.
3. `sclang tests/sc/verify_scdoc.scd` — indexes and renders all 135 help pages, failing on any warning.
4. `python3 tests/sc/generate_example_check.py build/examples.scd` — verifies SynthDef construction and audio stability across all classes.

### 6.5 Phase 5: Git Protocol
- Run `git status --short -- 03-jumble-science/MIR2` to inspect modified and untracked files.
- Stage only files within `03-jumble-science/MIR2/` using explicit paths:
  ```sh
  git add 03-jumble-science/MIR2/sounds/
  git add 03-jumble-science/MIR2/Classes/MIR2GUI.sc
  git add 03-jumble-science/MIR2/HelpSource/
  git add 03-jumble-science/MIR2/README.md
  git add 03-jumble-science/MIR2/docs/
  git add 03-jumble-science/MIR2/MIR2_DOCUMENTATION_AND_VISUALIZATION_PLAN.md
  git commit -m "MIR2: Add CC0 audio corpus, MIR2GUI live toolkit, and musical documentation plan"
  ```
- Under no circumstances run `git add .` or touch `03-jumble-science/SCPhysicalModels/`.

---

## 7. Summary of Changes in Directives for Project `.md` Files

The following records the explicit directive changes that have been added to the project documentation:

1. **`AGENTS.md` Override for MIR2**: The universal default rule to loop `ExampleFiles.child` is overridden for MIR2. MIR2 examples now use the dedicated dry CC0 suite in `03-jumble-science/MIR2/sounds/` with a graceful fallback to an internal synthetic probe when the sound file is not present.
2. **`README.md` & `docs/roadmap-status.md` Update**: Explicitly document the addition of the CC0 dry audio bundle, the `MIR2GUI` live visualization toolkit, and the complete musical overhaul of all 135 UGen help examples.
