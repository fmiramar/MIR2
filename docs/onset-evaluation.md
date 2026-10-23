# Onsets2 implementation audit and synthetic evaluation

The original implementation had important algorithmic and trigger-stage defects. Method selection alone could not fix them. The Astra High review identified a raw-bin logarithmic frontend in place of SuperFlux's quarter-tone filter bank, a neighborhood radius with a different meaning from the reference implementation, and repeated above-threshold triggers without peak selection. The argument named `medianTime` controlled exponential statistics rather than a median. Absolute flux also counted releases.

The revised native code has a log1p quarter-tone frontend, explicit band-neighborhood width, a true causal running median, one-hop local-maximum confirmation, rearming below half-threshold, calibrated window-RMS silence suppression, and a minimum novelty normalization scale. SuperFlux rejects a frame with a greater-than-10% RMS drop to reduce abrupt release artifacts. That last rule is a MIR2 heuristic and may reject an onset hidden under a simultaneous stronger release. All seven classical Onsets methods are exposed alongside the three original MIR2 symbols and HFC.

## Classical implementation review

The source examined was SuperCollider 3.14.1 `server/plugins/onsetsds.c` and `Onsets.cpp`. The review found these concrete differences from the intended formulas:

- Complex distance omitted the factor two on its cross term. MIR2 uses the full squared Euclidean distance.
- A phase-history read pointer advanced only for bins passing the magnitude gate, while writes advanced for every bin. MIR2 always advances every bin's history.
- Median-history shifting used overlapping `memcpy`. MIR2 uses a circular history and a separate scratch array.
- MKL's `whiten=false` initialization was ineffective because whitening dispatch checked a different field. MIR2 bypasses whitening for MKL explicitly.
- The wrapper computed whitening decay using an assumed half-window hop. MIR2 uses the declared matching FFT hop.

These are independently implemented fixes; no OnsetsDS source is embedded in MIR2. The original native source is compiled only as an optional external benchmark oracle, with its original behavior and defects preserved. Numeric ODF scaling and thresholds differ intentionally.

## Reproducible measurements

`tests/cpp/MIR2OnsetTests.cpp` generates deterministic five-second synthetic percussion, soft harmonic attacks, a sustained vibrato tone, repeated three-note mixtures, digital silence, very quiet noise, and an abrupt release. It computes periodic-Hann FFT frames at 44.1/48/96 kHz, FFT size 2048, and hops 0.25/0.5. It compares revised code, the original MIR2 0.1 detector, and optionally the external native OnsetsDS source. Thresholds 0.15/0.3/0.5/0.7 are swept separately for each detector/method. Ground truth consists of scheduled physical attacks. A greedy one-to-one match accepts detections within 80 ms, without subtracting latency. Unmatched detections are false positives and unmatched attacks are misses.

The table selects each method's best aggregate threshold on these same fixtures. This is a diagnostic sweep, not a held-out accuracy estimate or a claim that these thresholds generalize to recordings. Timing is mean absolute error for matched events and includes analysis delay. The CSV includes all methods.

| Implementation | Method | Threshold | TP | FP | FN | F1 | Mean timing error, ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| native_3.14.1 | rcomplex | 0.15 | 74 | 12 | 46 | 0.718 | 10.9 |
| native_3.14.1 | wphase | 0.15 | 72 | 12 | 48 | 0.706 | 10.8 |
| revised | rcomplex | 0.7 | 120 | 99 | 0 | 0.708 | 27.5 |
| revised | superflux | 0.7 | 120 | 10 | 0 | 0.960 | 29.8 |
| revised | wphase | 0.7 | 120 | 16 | 0 | 0.938 | 27.2 |
| v0.1 | superflux | 0.7 | 120 | 1115 | 0 | 0.177 | 18.2 |

At the default 0.3 threshold, revised SuperFlux detected all 120 scheduled attacks across these fixtures and settings. Residual false positives remain on vibrato, mixtures, and releases; this is documented rather than hidden by tuning each file separately. Quiet-noise suppression is partly the explicit RMS floor, and lowering that floor changes the result.

Standalone analytical checks also cover complex-distance identities, maximum-filter suppression of a one-band trajectory, finite bounded novelty, silent/quiet-noise rejection, and exact percussion event counts. The baseline threshold stage and the new stage are both exercised on identical FFT frames, while the direct kernel identities isolate several formula-level errors.

### Optional native comparison build

With `SC_SOURCE` set to the external SuperCollider 3.14.1 source directory:

```sh
cc -O2 -c "$SC_SOURCE/server/plugins/onsetsds.c" -o build/native-onsets.o
c++ -std=c++17 -O2 -DMIR2_NATIVE_ONSETS -Iinclude \
  -I"$SC_SOURCE/server/plugins" tests/cpp/MIR2OnsetTests.cpp \
  build/native-onsets.o -o build/onset-comparison
build/onset-comparison > build/onset-comparison.csv
```

No reference source is copied into this repository. The native file includes an overlapping-copy defect, so native benchmark results are compiler-dependent. The revised implementation's own sanitizer checks must be run without linking that known-defective oracle.

## Remaining quality work

These tests do **not** replace annotated recordings or listening. Real speech, singing, bowed strings, dense polyphonic recordings, low-level entrances, amplitude modulation, and overlapping release/attack cases still need a licensed listening/annotation corpus and independently tuned thresholds. CPU and musical performance across the entire FFT-size range also need further evaluation. The public audio example makes individual detections audible, but no human listening approval is claimed. The broad library roadmap remains open until its additional families and these evaluation gates are complete.

## Primary references

- [Dixon (2006), Onset Detection Revisited](https://www.dafx.de/paper-archive/2006/papers/p_133.pdf).
- [Böck and Widmer (2013), Maximum Filter Vibrato Suppression for Onset Detection](https://www.dafx.de/paper-archive/2013/papers/09.dafx2013_submission_12.pdf).
- [Authors' SuperFlux reference](https://github.com/CPJKU/SuperFlux): reviewed for filter-bank, lag and neighborhood conventions; no implementation copied.
- [SuperCollider 3.14.1 OnsetsDS](https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/onsetsds.c).
