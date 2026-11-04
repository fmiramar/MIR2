# ChromaCQT research gate

Decision: **include an explicitly limited native ChromaCQT**, sharing the existing
CQT kernel engine. The comparison establishes a useful low-register option with
longer windows, not general superiority over HPCP or recorded-music accuracy.
The public unit returns twelve folded powers directly, avoiding a large graph of
intermediate CQT outputs and language-side folding arithmetic.

## Reproducible comparison

`benchmarks/chroma_gate.cpp` compares production MIRa CQT, HPCP and FFT-chromagram
kernels. Build from the project directory with:

```sh
c++ -std=c++17 -O2 -Iinclude benchmarks/chroma_gate.cpp -o /tmp/mir2-chroma-gate
/tmp/mir2-chroma-gate > /tmp/mir2-chroma-gate.csv
```

The checked-in `benchmarks/chroma-gate-summary.csv` contains 400 rows. Each row
averages twelve chromatic transpositions. Cases include single sinusoids,
semitone pairs, equal-amplitude major triads, four-harmonic triads (amplitude 1/h),
and a phase-continuous high-register semitone change. Rates are 44.1, 48 and
96 kHz. Both A4=440 and A4 raised by 25 cents are tested, with the correct tuning
provided to all methods; this tests tuning support, not automatic tuning recovery.

CQT configurations are five octaves, fmin=55 with 12 bins/octave and fmin=110
with 24 or 36 bins/octave. Configurations requiring more than 32768 samples are
omitted, including 36 bins/octave at 96 kHz. The 12-bin low-register scenes start
at C2 and the finer grids at C3, so **scores across grid densities also change
register and must not be interpreted as an isolated density experiment**.

CQT powers are assigned to the nearest semitone (ties upward) and L1 normalized.
Both FFT routes use periodic Hann windows, the same frequency range and tuning,
and 4096 or 8192 samples; 8192 is the actual public MIRa FFT limit. HPCP uses one
harmonic hypothesis, squared-cosine weighting, a 4/3-semitone window and relative
peak threshold 0.01. This avoids injecting speculative subharmonics into the
known pure-tone fixtures. These are transparent comparison settings, not tuned
optimal settings for arbitrary recordings. The benchmark uses the production folding helper; a separate integer-assignment
and direct-correlation oracle checks its implementation. Half-semitone ties use
a 4e-6-semitone upward tolerance for float32 SynthDef rounding.

All methods analyze a trailing frame ending at the same sample. Their centre
ages differ. The semitone change occurs 20 ms before the common CQT centre;
shorter FFT windows therefore see a later part of the signal. This case compares
what is available at the same processing deadline, not equal-time centres.
Steady scenes do not have this alignment ambiguity.

Two scores are reported, both on [0,1]: target pitch-class mass (how concentrated
the output is on the known notes) and cosine similarity to the equal-weight
pitch-class template (also penalizing unequal allocation among target notes).
The harmonic-triad target remains its fundamental triad, so upper harmonics can
reduce either score. Neither metric is chord classification accuracy.

## Measured benefits and counterexamples

Representative A4=440 cosine similarities:

| Configuration and scene | Folded CQT | HPCP 8192 | FFT chromagram 8192 | CQT / FFT centre age |
| --- | ---: | ---: | ---: | ---: |
| 48 kHz, 55 Hz / 12 bins, C2 semitone pairs | 0.9835 | 0.8993 | 0.8839 | 170.7 / 85.3 ms |
| 48 kHz, 55 Hz / 12 bins, C2 major triads | 0.9322 | 0.9999 | 0.8921 | 170.7 / 85.3 ms |
| 96 kHz, 55 Hz / 12 bins, C2 major triads | 0.9322 | 0.5250 | 0.5528 | 170.7 / 42.7 ms |
| 48 kHz, 110 Hz / 36 bins, C3 semitone pairs | 1.0000 | 0.9619 | 0.9836 | 341.3 / 85.3 ms |
| 48 kHz, 110 Hz / 24 bins, C3 single tones | 0.9771 | 1.0000 | 0.9602 | 170.7 / 85.3 ms |

The 48 kHz C2 pair result also shows why both metrics matter: HPCP target mass
is higher (0.9653 versus 0.8527), but CQT distributes energy across the two notes
more evenly. A single “winning” score would conceal that tradeoff. HPCP is better
on C2 major triads at 48 kHz and on many isolated tones. At 96 kHz its fixed FFT
sample limit provides less low-frequency resolution. A longer-window or
resampled HPCP could narrow this difference; that is not the current public
implementation. The result does not prove an inherent advantage over all
possible FFT-based chroma methods.

The 12-bin Hann bank spreads an isolated note into neighboring pitch classes.
Fine grids improve isolation but cost more memory, preparation and latency;
even densities also place bins exactly at the nearest-semitone decision boundary.
The 36-bin example needs 32768 samples at 48 kHz and is unsupported at 96 kHz
with that minimum frequency. Increasing resolution is not a free quality gain.

## Implementation and verification

`ChromaCQT.kr` shifts the bank and reference together for the selected tuning,
folds power into twelve C-first outputs, optionally thresholds weak classes,
and normalizes by maximum, sum or Euclidean norm. Default tuning is 440 Hz,
fmin=55, five octaves and 12 bins/octave. All settings are fixed at construction.
The existing sparse-kernel memory cap, 32768-sample frame bound and Nyquist
margin apply. Callbacks allocate no memory. Startup is one common frame, and
results refer to its centre, then hold between hops.

`MIRaChromaCQTTests.cpp` independently tests semitone assignment with integer
arithmetic for all four supported densities and three tunings, octave summation,
threshold/normalization behavior, exact silence, and direct time-domain complex
correlation followed by folding at three sample rates. The broader CQT oracle
already tests sparse-kernel accuracy and invalid configurations. The help page
contains an audible pitch-class-driven oscillator bank using ExampleFiles.child.

Recorded chord/key accuracy, tuning-estimation error, adversarial noise and
cross-platform CPU guarantees remain unmeasured. Reopen the comparison on
annotated material before recommending ChromaCQT as a universal default.

## Sources

The transform follows the Brown/Brown–Puckette sources listed in
[the constant-Q implementation report](wavelet-constantq-implementation.md).
The pitch-class interpretation and low-register FFT-resolution issue are
explained in [Mueller's FMP chromagram notebook](https://www.audiolabs-erlangen.de/resources/MIR/FMP/C3/C3S1_SpecLogFreq-Chromagram.html).
No external implementation code or audio dataset was imported.
