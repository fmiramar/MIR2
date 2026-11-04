# Onsets2 implementation audit and synthetic evaluation

The original implementation had important algorithmic and trigger-stage defects. Method selection alone could not fix them. The Astra High review identified a raw-bin logarithmic frontend in place of SuperFlux's quarter-tone filter bank, a neighborhood radius with a different meaning from the reference implementation, and repeated above-threshold triggers without peak selection. The argument named `medianTime` controlled exponential statistics rather than a median. Absolute flux also counted releases.

The revised native code has a log1p quarter-tone frontend, explicit band-neighborhood width, a true causal running median, one-hop local-maximum confirmation, rearming below half-threshold, calibrated window-RMS silence suppression, and a minimum novelty normalization scale. SuperFlux suppresses a frame when its RMS falls below the previous RMS multiplied by `exp(-hopSeconds / releaseTime)`. The default release time is 0.15 seconds; zero disables this guard. This seconds-based MIRa heuristic reduces abrupt release artifacts but may reject an onset hidden under a simultaneous stronger release. All seven classical Onsets methods are exposed alongside the three original MIRa symbols and HFC.

## Classical implementation review

The source examined was SuperCollider 3.14.1 `server/plugins/onsetsds.c` and `Onsets.cpp`. The review found these concrete differences from the intended formulas:

- Complex distance omitted the factor two on its cross term. MIRa uses the full squared Euclidean distance.
- A phase-history read pointer advanced only for bins passing the magnitude gate, while writes advanced for every bin. MIRa always advances every bin's history.
- Median-history shifting used overlapping `memcpy`. MIRa uses a circular history and two indexed heaps for an exact running median with logarithmic update cost.
- MKL's `whiten=false` initialization was ineffective because whitening dispatch checked a different field. MIRa bypasses whitening for MKL explicitly.
- The wrapper computed whitening decay using an assumed half-window hop. MIRa rounds the declared matching FFT hop to the server block size, as FFT does. The full FFT window is required; a separately shortened FFT `winsize` configuration is not inferred.

These are independently implemented fixes; no OnsetsDS source is embedded in MIRa. The original native source is compiled only as an optional external benchmark oracle, with its original behavior and defects preserved. Numeric ODF scaling and thresholds differ intentionally.

## Reproducible measurements

`tests/cpp/MIRaOnsetTests.cpp` generates deterministic five-second synthetic percussion, soft harmonic attacks, a sustained vibrato tone, repeated three-note mixtures, digital silence, very quiet noise, and an abrupt release. It computes periodic-Hann FFT frames at 44.1/48/96 kHz, FFT size 2048, and hops 0.25/0.5. It compares revised code, the original MIRa 0.1 detector, and optionally the external native OnsetsDS source. Thresholds 0.15/0.3/0.5/0.7 are swept separately for each detector/method. Ground truth consists of scheduled physical attacks. A greedy one-to-one match accepts detections within 80 ms, without subtracting latency. Unmatched detections are false positives and unmatched attacks are misses.

The table selects each method's best aggregate threshold on these same fixtures. This is a diagnostic sweep, not a held-out accuracy estimate or a claim that these thresholds generalize to recordings. Timing is mean absolute error for matched events and includes analysis delay. The CSV includes all methods.

| Implementation | Method | Threshold | TP | FP | FN | F1 | Mean timing error, ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| native_3.14.1 | mkl | 0.15 | 104 | 12 | 16 | 0.881 | 10.0 |
| native_3.14.1 | phase | 0.15 | 115 | 25 | 5 | 0.885 | 10.8 |
| revised | complex | 0.7 | 120 | 12 | 0 | 0.952 | 37.7 |
| revised | phase | 0.7 | 112 | 168 | 8 | 0.560 | 27.3 |
| revised | rcomplex | 0.7 | 120 | 125 | 0 | 0.658 | 32.3 |
| revised | superflux | 0.7 | 120 | 8 | 0 | 0.968 | 34.6 |
| revised | wphase | 0.7 | 120 | 16 | 0 | 0.938 | 30.7 |
| v0.1 | superflux | 0.7 | 120 | 1115 | 0 | 0.177 | 18.2 |

At the default 0.3 threshold, revised SuperFlux detected all 120 scheduled attacks with 24 false positives across these fixtures and settings (F1 0.909), compared with 1371 false positives for the original MIRa implementation (F1 0.149). Residual false positives remain on vibrato, mixtures, and releases; this is documented rather than hidden by tuning each file separately. Quiet-noise suppression is partly the explicit RMS floor, and lowering that floor changes the result.

Corrected formulas do not ensure better event-level accuracy for every method: phase and rectified-complex methods still fare poorly on this fixture set. Their thresholds and peak selection need separate musical evaluation.

Standalone analytical checks also cover complex-distance identities, maximum-filter suppression of a one-band trajectory, finite bounded novelty, silent/quiet-noise rejection, and exact percussion event counts. The baseline threshold stage and the new stage are both exercised on identical FFT frames, while the direct kernel identities isolate several formula-level errors.

### Optional native comparison build

With `SC_SOURCE` set to the external SuperCollider 3.14.1 source directory:

```sh
cc -O2 -c "$SC_SOURCE/server/plugins/onsetsds.c" -o build/native-onsets.o
c++ -std=c++17 -O2 -DMIRa_NATIVE_ONSETS -Iinclude \
  -I"$SC_SOURCE/server/plugins" tests/cpp/MIRaOnsetTests.cpp \
  build/native-onsets.o -o build/onset-comparison
build/onset-comparison > build/onset-comparison.csv
python3 tests/python/summarize_onsets.py build/onset-comparison.csv \
  benchmarks/onset-synthetic-summary.csv
```

No reference source is copied into this repository. The native file includes an overlapping-copy defect, so native benchmark results are compiler-dependent. The revised implementation's own sanitizer checks must be run without linking that known-defective oracle.

## Timing, memory, and update cost

`MIRaOnsetTimingTests` independently checks minimum-gap boundaries, the duration
of median windows at 44.1/48/96 kHz and FFT sizes 256–8192, hop rounding to blocks
16/64/128, and all methods at default/example FFT sizes. The minimum gap includes
the current frame elapsed time, so an onset exactly at the requested boundary is
accepted. A history requiring more than 65,536 frames is rejected rather than
silently shortened. Three frames are retained even for a zero median time.

A local optimized x86_64 run measured median updates at 0.039 microseconds for
three entries and 0.138 microseconds for 65,536 entries (largest observed
1024-update batch means 0.069 and 0.269 microseconds). SuperFlux with FFT size
256, a declared 16-sample hop at 96 kHz, and a 60,000-frame median history took
1.145 microseconds per frame, about 0.69% of one core's audio time. That kernel
measurement excludes FFT and server overhead. The test prints these costs for
reproduction; wall-clock values are observations, not hard realtime guarantees.

## Remaining quality work

These tests do **not** replace annotated recordings or listening. Real speech, singing, bowed strings, dense polyphonic recordings, low-level entrances, amplitude modulation, and overlapping release/attack cases still need a licensed listening/annotation corpus and independently tuned thresholds. Musical accuracy across the entire FFT-size range remains unevaluated. The public audio example makes individual detections audible, but no human listening approval is claimed. On 2026-10-23 the user explicitly deferred the representative-recording test to a later stage; it does not block completion of the broader implementation pass.

## Primary references

- [Dixon (2006), Onset Detection Revisited](https://www.dafx.de/paper-archive/2006/papers/p_133.pdf).
- [Böck and Widmer (2013), Maximum Filter Vibrato Suppression for Onset Detection](https://www.dafx.de/paper-archive/2013/papers/09.dafx2013_submission_12.pdf).
- [Authors' SuperFlux reference](https://github.com/CPJKU/SuperFlux): reviewed for filter-bank, lag and neighborhood conventions; no implementation copied.
- [SuperCollider 3.14.1 OnsetsDS](https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/onsetsds.c).
