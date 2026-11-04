# Tonal and harmonic implementation

Twenty-five public classes implement tonal and harmonic analysis independently.
The shared peak extractor uses parabolic interpolation and a relative magnitude
floor. FFT frames are read without changing their complex/polar representation.
The tonal reference is [Gómez (2006)](https://doi.org/10.1287/ijoc.1040.0126);
[Harte, Sandler and Gasser (2006)](https://ofai.at/~martin.gasser/papers/oefai-tr-2006-13.pdf)
supplies the six-dimensional tonal-centroid geometry. Individual class help gives
arguments, formulas, units and silence/startup behavior.

## Pitch classes, keys and chords

HPCP is C-first and supports 12, 24, 36 or 120 bins. Peaks contribute squared
magnitudes to possible harmonic fundamentals, with harmonic weights 0.6^(h-1),
nearest/cosine/squared-cosine pitch-class weighting and explicit none/max/L1/L2
normalization. This is a documented MIRa harmonic weighting convention, not a
claim of exact upstream HPCP equivalence. `Chromagram2` can instead distribute
FFT-bin power by linear pitch-class interpolation. `TuningFrequency` estimates
the circular mean of peak detuning modulo a semitone and exposes concentration
as confidence; that estimate is ambiguous by multiples of a semitone.

`ChromaCENS` performs L1 normalization, four-level quantization at
0.05/0.1/0.2/0.4, a causal trailing Hann average and L2 normalization, with explicit
downsampling. Unlike a centered offline CENS extractor it waits for no future
frames. `Tonnetz` projects normalized chroma onto fifth, minor-third and
major-third circles with radii 1, 1 and 0.5. `HarmonicChange` measures the distance
between two frames around the preceding frame, so its change score is delayed
one update. `HPCPEntropy` and `HPCPCrest` operate directly on supplied vectors.

`KeyTrack2`, `KeyClarity2` and `KeyMode2` use normalized correlation against all
24 rotations of the selected major/minor profile. The actual numeric profiles
are Krumhansl/Kessler (1982), Temperley (1999) and Faraldo et al. EDMA (2016),
identified in the class help and kernel. They are research profile numbers,
not imported implementation code. A silent current frame has no key even while
the context decays. Causal smoothing controls context and is not a global key
annotation algorithm.

`ChordTrack` and `ChordStrength` compare cosine templates with an explicit
weakest-tone coverage factor, then use causal min-sum dynamic programming with
a transition penalty and a no-chord state. The default `\triads` vocabulary
preserves major/minor/no-chord behavior. The opt-in `\extended` vocabulary
contains 109 states: twelve roots for nine qualities, plus no-chord. Quality
indices are 0 major, 1 minor, 2 diminished, 3 augmented, 4 dominant seventh,
5 major seventh, 6 minor seventh, 7 sus2 and 8 sus4. `ChordStats` uses the same
vocabulary to normalize duration entropy by log(25) or log(109). No allocations
occur during updates; state arrays have fixed maximum capacity.

The optional vocabulary gate passed on independently specified musical
pitch-class masks for all 108 complete templates, incomplete-chord rejection,
silence, delayed changes under a transition penalty and unchanged default
major/minor behavior. Symmetric labels are deliberately not assigned fake
certainty: augmented roots four semitones apart and equivalent sus2/sus4
rotations have identical chroma. Equal-cost new paths select lower quality,
then lower root; an existing path can preserve an equivalent label. Chroma
cannot determine inversion or bass position. These tests establish vocabulary
and state-machine correctness, not recording-based recognition accuracy.

`ChordTrack.qualityNames` and `ChordTrack.label` expose language-side labels.
The three chord help examples exercise the extended vocabulary audibly. The
additional test executable `MIRaExtendedChordTests.cpp` uses independent bitmask
templates and reports an optimized-kernel microbenchmark, excluding SC overhead.

## Harmonic descriptors

`HarmonicPeaks` associates the strongest peak near each multiple of a supplied
F0 with a configurable cents tolerance. Passing an independent F0 keeps harmonic
analysis separate from pitch estimation and allows controlled reference tones.
`Inharmonicity` is power-weighted absolute frequency deviation divided by F0.
`Tristimulus` splits amplitude into harmonic 1, harmonics 2–4, and the remainder.
`OddEvenRatio` uses harmonic power, capped at 1e6 when even power vanishes.
`HarmonicCentroid`/`HarmonicSpread` are amplitude-weighted frequency moments.
`HarmonicDeviation` measures amplitude-weighted absolute dB deviation from a
three-harmonic local envelope. `HarmonicVariation` is one minus the cosine
similarity of successive harmonic-amplitude vectors.

`HarmonicEnergyRatio` counts FFT power within one bin of associated peaks,
counting each bin once, divided by whole-spectrum power. `Noisiness` is its
complement for active frames and zero for silence. These are spectral feature
conventions, not a separated harmonic/noise audio model. `SensoryDissonance2`
uses pairwise Sethares-style roughness with the stated frequency-separation
curve and selectable minimum-amplitude or amplitude-product weighting. Peak
normalization is explicit; this feature is not a universal perceptual score.

## Verification and resources

`tonal_harmonic_tests.cpp` exercises all pitch-class rotations and tunings at
all four resolutions, 72 key-profile cases, 24 major/minor chords and no-chord,
analytic entropy/Tonnetz/CENS/roughness values, stretched harmonics, and peak
extraction at 44.1/48/96 kHz with FFT sizes 2048/4096/8192. A maximum-configured
HPCP kernel measured about 0.289 ms/frame in a local optimized run; costs depend
on peak/harmonic/bin counts and exclude server overhead. RT memory is allocated
in construction, fixed-capacity peak/harmonic arrays bound per-frame work, and
callbacks do not allocate. Each public class has an analysis-driven audible
example included in the installed graph/audio verification. Numerical tests
and demos do not establish general key/chord/transcription accuracy.
