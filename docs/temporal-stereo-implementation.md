# Temporal, stereo, and feature dynamics implementation

These 25 analyzers implement the temporal/envelope, stereo, and small feature-dynamics families in the MIR2 roadmap. They are independent C++ implementations, with no Essentia or HTK source copied into MIR2.

## Definitions and scope

The rolling envelope analyzers measure nonoverlapping RMS frames. Their observation windows are rounded upward to complete RMS frames and outputs remain zero until a complete window is available. Time coordinates run from the oldest frame boundary toward the present, using frame centres. `TemporalCentroid`, `EnvelopeSpread`, `EnvelopeSkewness`, and `EnvelopeKurtosis` use squared RMS weights and standard central moments. Kurtosis is Pearson kurtosis, without subtracting three.

`EffectiveDuration` is the fraction of frames at least 10% of the window's largest RMS amplitude. `SilenceRate` instead uses an absolute dBFS threshold. `DynamicComplexity` is the RMS dB distance of nonsilent frames from the power-mean level. `StrongDecay` is the square root of integrated energy divided by its temporal centroid. The latter follows the energy/centroid idea of Gouyon and Herrera, “Exploration of techniques for automatic labeling of audio drum tracks instruments,” MOSART 2001, as described by the [Essentia StrongDecay reference](https://essentia.upf.edu/reference/streaming_StrongDecay.html). Its RMS energy convention is explicitly a MIR2 rolling adaptation; neither it nor `DynamicComplexity` claims Essentia numerical equivalence.

`TemporalDecrease` fits RMS amplitude against time and returns the negative slope divided by mean amplitude. `FadeDetection` fits dB level against time and returns `[direction, dBPerSecond, rSquared]`; direction is gated by slope and fit thresholds. These are explicit MIR2 conventions where the plan did not specify a unique formula.

`AttackTime`, `AttackSlope2`, `LogAttackTime`, and `DecayTime` deliberately accept an external event trigger. Each rising edge captures a fixed window, restarting an unfinished capture. Completed captures identify the largest RMS peak and interpolate relative low/high threshold crossings. Attack and decay durations are measured between these crossings, not between the trigger and the peak. Attack slope is the threshold amplitude difference divided by attack duration. A truncated/unresolvable interval returns zero. `LogAttackTime` returns seconds and the base-ten logarithm, with both outputs zero when invalid. This fixed capture design has predictable bounded latency but requires the caller to choose event duration and segmentation; it does not discover event boundaries itself.

The time-domain stereo family uses exact rolling sample sums. `StereoCorrelation` subtracts channel means and implements Pearson correlation; `StereoBalance` is `(rightEnergy-leftEnergy)/totalEnergy`; `StereoWidth` is `sqrt(sideEnergy/(midEnergy+sideEnergy))`; `MidSideRatio` is the mid/side energy ratio in dB, limited to ±120. Channels are M=(L+R)/2 and S=(L-R)/2. Sums are recalculated once per window to bound long-running roundoff drift.

The frequency-domain stereo family maintains two Hann-windowed FFTs and exponentially averages per-bin power/cross spectra. `StereoCoherence` applies the magnitude-squared coherence definition, with band averaging weighted by shared channel energy. See the [MathWorks cross-spectrum definition](https://www.mathworks.com/help/signal/ref/mscohere.html). At least four frames are collected before publishing coherence; longer averaging is still needed for a stable stochastic estimate. `StereoPanning` uses `(rightMagnitude-leftMagnitude)/(rightMagnitude+leftMagnitude)` per bin with total-energy band weights. `InterchannelPhase` reports the argument and concentration of summed averaged cross spectra L*conj(R). A low concentration means a single band phase is ambiguous; this is not a delay estimator. All three accept frequency limits, and empty bands return zero.

`FeatureDelta` implements the symmetric regression delta described in the [HTK Book, §5.6](https://labrosa.ee.columbia.edu/doc/HTKBook21/node56.html), expressed per second. `FeatureDeltaDelta` applies the same regression twice. Their causal implementation delays the centre estimate by N and 2N accepted feature updates respectively. An explicit trigger lets all utilities follow feature updates rather than repeatedly treating a held feature as a new frame. The user must supply the correct fixed update period. `FeatureMedian` is a trailing order statistic; `FeatureSmooth` uses a one-pole recurrence with a 1/e time constant.

## Bounds and cost

All ring buffers, FFT arrays, envelope storage, and sorting scratch space are allocated with RTAlloc in constructors and freed in destructors. Callbacks do not allocate or lock. Invalid/nonfinite source samples are zeroed, and audio analysis inputs are bounded to ±1e6 to avoid pathological floating-point ranges. Audio inputs are required for temporal/stereo analyzers; feature utilities accept scalar/control inputs.

| Family | Structural limits | First complete result / group delay | Approximate processing work |
| --- | --- | --- | --- |
| Rolling envelope | 1–100 ms RMS frames; window up to 10 s; at most 10,000 frames | Whole observation window | O(envelope frames) per RMS update |
| Triggered event | Same frame/window bounds | Whole capture after trigger | O(capture frames) per completed event |
| Time-domain stereo | 1 ms–2 s sample window | Whole observation window | O(block size), with O(window samples) periodic rebase |
| Frequency stereo | FFT 256–8192, hop 1/16–1 frame | One FFT frame; coherence adds three hops; averaging then settles | Two O(N log N) FFTs per hop |
| Regression feature delta | Radius 1–32 | N updates delay; 2N+1 values before output | O(radius) per update |
| Regression delta-delta | Radius 1–32 | 2N updates delay; 4N+1 values before output | O(radius) per update |
| Feature median | Window 1–127 | Full trailing window | O(window log window) per update |
| Feature smoother | One state value | First accepted value | O(1) per update |

The comprehensive 1,000-frame envelope statistics kernel measured approximately 10.8 microseconds per update in an optimized local run (2,000 iterations). At 100 updates/second that is approximately 0.11% of one core per analyzer for the kernel alone. This measurement is illustrative and excludes server scheduling; it is not a cross-platform performance guarantee. Larger windows or 1 ms frame updates can be substantially more expensive. Every scalar envelope UGen currently computes the complete statistics set, favoring consistent definitions over minimal arithmetic.

## Verification and integration

`tests/cpp/MIR2TemporalStereoTests.cpp` checks analytical time moments, silence/active ratios, an exact exponential dB fade, threshold crossing durations, truncated-event behavior, Pearson invariance to DC, M/S extremes, actual cross-spectrum averaging, phase sign, frequency-selective panning, regression slope/acceleration, median outlier rejection, and one-pole timing. Optimized and AddressSanitizer/UndefinedBehaviorSanitizer runs pass. The three plugin translation units pass strict standalone syntax compilation against the SuperCollider 3.14.1 headers. An isolated SuperCollider 3.14.1 language run compiled the class library, constructed all 25 example SynthDefs, indexed SCDoc with `SCDoc.indexAllDocuments(true)`, and rendered all 25 changed class pages without warnings or errors. Global installation and integrated server checks are handled by the main library integration.

Integration requires adding `plugins/MIR2Temporal.cpp`, `plugins/MIR2Stereo.cpp`, and `plugins/MIR2FeatureDynamics.cpp` to the server target; adding the matching three `Classes/MIR2*.sc` files to installation; and declaring/calling `registerTemporalUnits()`, `registerStereoUnits()`, and `registerFeatureDynamicsUnits()`. The test file is a standalone executable and can be registered with CTest. All 25 public classes have an audible help example driven by the measured feature, using the looping bundled SuperCollider example file and a short output fade-in. The project guide should list the classes together; no additional project guide is needed.
