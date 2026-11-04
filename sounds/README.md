# MIRa Example Audio Corpus

This directory contains clean, dry Creative Commons Zero (CC0 1.0 Universal) audio recordings curated for the SuperCollider MIRa analysis library. These recordings provide musically meaningful source material for pitch, tonal, rhythm, onset, harmonic, spectral, stereo, and temporal feature extractors, replacing the speech-only default clip (`ExampleFiles.child`).

Although the CC0 1.0 Universal Public Domain Dedication permits copying, modifying, and distributing these works without requesting permission or providing attribution, MIRa documents and credits all original authors below.

All files are formatted as **44,100 Hz, 16-bit PCM WAV, peak-normalized to -1.0 dBFS**, with 5 ms linear zero-crossing edge fades to prevent loop boundary clicks.

---

## 1. `drums.wav`

- **Content**: Dry acoustic drum kit groove at 120 BPM across 4 bars. Tight kick on beats 1 and 3, snappy snare on beats 2 and 4 with ghost notes, steady 8th-note hi-hat, open hat variation, and a clean 1-bar ending fill.
- **Musical Attributes**: 120.0 BPM, 4/4 time, 4 complete bars, loopable.
- **Duration**: 8.000 seconds (352,800 samples). Channels: 1 (mono).
- **Original Titles**:
  - `Drums_120bpm_closedhat_snareghost_loop1.wav` ([Freesound 169870](https://freesound.org/people/tichec/sounds/169870/))
  - `Drums_120bpm_openhat_loop1.wav` ([Freesound 169869](https://freesound.org/people/tichec/sounds/169869/))
  - `Drums_120bpm_fill_loop1.wav` ([Freesound 169872](https://freesound.org/people/tichec/sounds/169872/))
- **Author**: `tichec`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Concatenated 2 bars of closed-hat/ghost pattern (4.0s), 1 bar of open-hat (2.0s), and 1 bar of fill (2.0s); mixed to mono; resampled to 44.1 kHz; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `BeatTrack3`, `Tempo`, `Tempogram`, `RhythmPattern`, `RhythmTransform`, `Onsets2`, `FFTMKL2`, `DynamicComplexity`, `OnsetRate`.

---

## 2. `piano-chords.wav`

- **Content**: Clean, dry acoustic piano playing a sustained chord progression in A minor ($A\text{m} - F - C - G$), with clear voice leading and sustained ring.
- **Musical Attributes**: Key of A minor / C major; diatonic triad and seventh voicings; clear harmonic transitions.
- **Duration**: 9.720 seconds (428,652 samples). Channels: 1 (mono).
- **Original Title**: `A Minor Progression Piano.wav` ([Freesound 238328](https://freesound.org/people/johnthewizar/sounds/238328/))
- **Author**: `johnthewizar`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Resampled from 48 kHz to 44.1 kHz; mixed to mono; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `HPCP`, `Chromagram2`, `KeyTrack2`, `KeyClarity2`, `KeyMode2`, `ChordTrack`, `ChordStrength`, `Tonnetz`, `ChromaCENS`, `ChromaCQT`, `SensoryDissonance2`.

---

## 3. `melody.wav`

- **Content**: Dry acoustic saxophone solo playing an expressive lyrical phrase with intentional vibrato, smooth portamento glides, and dynamic pitch variation across two octaves.
- **Musical Attributes**: Monophonic sustained pitch ($D_4 \approx 293$ Hz up to $B_5 \approx 987$ Hz), strong harmonic series, natural 4–6 Hz vibrato.
- **Duration**: 12.000 seconds (529,200 samples). Channels: 1 (mono).
- **Original Title**: `sax solo dry` ([Freesound 607303](https://freesound.org/people/DieRechnug/sounds/607303/))
- **Author**: `DieRechnug`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Trimmed 12.0s excerpt (offset 24.0s to 36.0s); resampled from 48 kHz to 44.1 kHz; mixed to mono; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `PitchYIN`, `PitchMPM`, `Pitch2`, `PitchSWIPE`, `PitchContour`, `Vibrato2`, `HarmonicPeaks`, `Tristimulus`, `Inharmonicity`, `OddEvenRatio`, `HarmonicCentroid`.

---

## 4. `plucks.wav`

- **Content**: 4 distinct struck/plucked acoustic events with steep transient attacks and natural exponential ring-downs separated by clean silence:
  1. Glockenspiel bell strike ($F\sharp_4$) with metallic ring and high-frequency inharmonicity.
  2. Kalimba thumb piano tine pluck with wood/metal resonance.
  3. Nylon guitar pluck note 1 with warm string resonance.
  4. Nylon guitar pluck note 2 with high harmonic clarity.
- **Musical Attributes**: Isolated transient onsets at 0.0s, 3.0s, 5.5s, and 8.0s. Natural decay times from 1.5s to 2.8s.
- **Duration**: 10.500 seconds (463,050 samples). Channels: 1 (mono).
- **Original Titles & Authors**:
  - `Glockenspiel - F#4` by `sgossner` / `Samulis` ([Freesound 373367](https://freesound.org/people/sgossner/sounds/373367/))
  - `Kalimba.wav` by `PanPiper5` ([Freesound 659909](https://freesound.org/people/PanPiper5/sounds/659909/))
  - `Nylon Plucks` by `bangcorrupt` ([Freesound 833157](https://freesound.org/people/bangcorrupt/sounds/833157/))
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Individual strikes trimmed, normalized, aligned with silence spacing, and concatenated; mixed to mono; resampled to 44.1 kHz; peak-normalized to -1.0 dBFS.
- **Primary Targets**: `AttackTime`, `AttackSlope2`, `LogAttackTime`, `DecayTime`, `EffectiveDuration`, `StrongDecay`, `TemporalCentroid`, `Inharmonicity`.

---

## 5. `stereo.wav`

- **Content**: 2-channel acoustic piano and percussion mix with wide spatial stereo movement, rotary panning modulation, and varying interchannel phase correlation.
- **Musical Attributes**: Stereo balance swings across $[-0.48, +0.16]$; interchannel correlation varies between $+0.34$ and $+0.64$; dynamic soundstage width.
- **Duration**: 12.000 seconds (529,200 samples). Channels: 2 (stereo).
- **Original Title**: `Spinning Piano Chords.wav` ([Freesound 539312](https://freesound.org/people/Jackie-makes-noiz/sounds/539312/))
- **Author**: `Jackie-makes-noiz`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Trimmed 12.0s excerpt (offset 10.0s to 22.0s); preserved 2 stereo channels; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `StereoBalance`, `StereoWidth`, `StereoCorrelation`, `MidSideRatio`, `StereoCoherence`, `StereoPanning`, `InterchannelPhase`.

---

## 6. `bass-melody-mix.wav`

- **Content**: Polyphonic texture containing an acoustic walking bassline in the low register ($55 \dots 140$ Hz) accompanied by upper melodic piano chords ($300 \dots 1200$ Hz).
- **Musical Attributes**: Clear two-part polyphony with distinct spectral separation between bass fundamentals and treble harmony.
- **Duration**: 9.500 seconds (418,950 samples). Channels: 1 (mono).
- **Original Title**: `Piano melody chords and walking bass` ([Freesound 727933](https://freesound.org/people/owstu/sounds/727933/))
- **Author**: `owstu`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Seamlessly looped and trimmed to 9.5s; mixed to mono; resampled to 44.1 kHz; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `MultiPitch`, `PredominantPitch`, `FundamentalCandidates`, `PitchProb`, `PitchAmbiguity`, `PitchConsensus`.

---

## 7. `texture.wav`

- **Content**: Continuous friction scraping and resonant bowing of an acoustic cymbal. High spectral density across the upper frequency range, stochastic noise texture, and natural metallic resonance.
- **Musical Attributes**: Broad spectral spread, high spectral flatness index, and sustained texture without rigid harmonic series.
- **Duration**: 10.000 seconds (441,000 samples). Channels: 1 (mono).
- **Original Title**: `BowedCymbal_1.wav` ([Freesound 333064](https://freesound.org/people/ceich93/sounds/333064/))
- **Author**: `ceich93`
- **License**: [Creative Commons 0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- **Processing**: Trimmed 10.0s steady-state section (offset 2.0s to 12.0s); mixed to mono; resampled to 44.1 kHz; peak-normalized to -1.0 dBFS; 5 ms edge fades.
- **Primary Targets**: `SpecFlatness2`, `SpecEntropy`, `SpecSparsity`, `Noisiness`, `SilenceRate`, `SpecComplexity`.
