# Multiband modulation research gate

The optional per-band rhythm gate is implemented as `RhythmTransform` and `RhythmPattern`. The distinct capability is retaining **which spectral band carries each modulation rate**. Summing envelopes before a tempogram loses that association. An independent two-envelope fixture with 2 Hz and 5 Hz amplitude modulation preserves both rates in separate rows, including their phase and amplitude, across nine sample-rate/block-size combinations.

`RhythmPattern` is explicitly a **MIR2 Bark/log-power adaptation**, not an exact reproduction of the canonical Rhythm Patterns descriptor. Its source relationship is Lidy and Rauber, [Evaluation of Feature Extractors and Psycho-Acoustic Transformations for Music Genre Classification, ISMIR 2005, section 3.1](https://ismir2005.ismir.net/proceedings/1033.pdf). That method motivates the per-band modulation representation, 4 Hz emphasis and smoothing. MIR2 omits spectral masking, phon/sone calibration, gradient filtering and whole-track median aggregation. No upstream implementation or data tables were copied. Genre-classification quality is not inferred from the numerical tests.

## Defined signal and layout

- `RhythmTransform.kr` accepts a literal array of 1–32 scalar/control envelopes. Each output row describes one input band, with a linear modulation-frequency grid. It retains signed inputs and optionally subtracts their Hann-weighted mean.
- `RhythmPattern.kr` composes existing `BarkBands` power output with `log(1 + gain * max(power, 0))`, always removes the weighted mean, and uses the same modulation kernel. `krBands` accepts externally prepared envelopes, with no compression by default.
- Magnitudes are flat, band-major: `band * numBins + bin`. Optional RhythmTransform phase channels are a complete second matrix after the magnitudes, with the same layout.
- The symmetric Hann window uses denominator `frames - 1`. Direct complex projection evaluates arbitrary modulation frequencies; no zero padding or FFT-bin rounding changes the requested grid. Magnitude is `2 * abs(projection) / sum(window)` except DC, which uses one rather than two. This is coherent-gain correction, not energy normalization.
- Phase is the cosine phase extrapolated to the newest pooled sample, wrapped to [-pi, pi]. The newest sample represents the center of its block-averaging interval. Tiny coefficients have phase zero.
- Optional fluctuation emphasis is the declared MIR2 function `2 / (f/4 + 4/f)`, zero at DC and unit gain at 4 Hz. It is not a calibrated fluctuation-strength measurement. Gaussian smoothing is separable across bands and bins, radius `ceil(3*sigma)` at most nine, with truncated-edge weight renormalization. L2 normalization is none, global, or per band.

## Streaming and bounds

A feature sample is the arithmetic mean of a rounded, fixed number of server control blocks. For sample rate `sr`, block size `bs`, and requested rate `r`, `pool = max(1, round((sr/bs)/r))` and actual rate `R = sr/(bs*pool)`. History is `ceil(windowTime*R)` frames, at least three; hop is `ceil(hopTime*R)` frames, at least one. The first analysis occurs exactly when history fills. Subsequent analyses use whole hops. There is no partial zero-padded startup estimate.

Held FFT features are treated as a zero-order-held envelope in elapsed time. Their repetitions do not create a fictitious sequence of new FFT events. Actual timing is independent of the upstream FFT hop, but the upstream hop still limits recoverable modulation bandwidth and adds its own latency. Block averaging is only a boxcar antialias filter. Users must keep the envelope bandwidth below the actual feature Nyquist rate; the requested highest modulation bin is constrained to 0.4 of the actual rate.

All configuration is fixed at construction. Input samples are made finite and bounded to ±1e12 before processing. Log compression gain is bounded to 1e6. Matrix outputs including optional phase must fit 1024 channels. `bands * bins * frames <= 524288` bounds the direct-projection work and storage products. Invalid configurations and failed RT allocations clear outputs and stop the unit. All history, basis and scratch storage is allocated in construction; processing performs no allocations, FFT-buffer writes, file I/O, or shared mutable work.

Increasing hopTime reduces average CPU but does not reduce the per-update computation burst. This matters when several instances update in the same server block. Reduce bands, bins or history when the maximum matrix exceeds the available audio-block budget.

## Verification

`tests/cpp/MIR2ModulationTests.cpp` checks:

- An independently constructed long-double complex DFT against every magnitude/phase channel, including non-integer frequencies, DC and mean removal.
- Two independent AM envelopes at 2/5 Hz: correct separate peak bins, amplitudes within 0.005, phase within 0.002 radians, and exact first/subsequent update times at 44.1/48/96 kHz with block sizes 32/64/128.
- Constant-envelope/DC behavior, complete stale-feature flushing, nonfinite input sanitation, compression, Gaussian edge normalization, whole-matrix/per-band L2 normalization, fluctuation-weight symmetry and dimension/rate rejection.

The focused suite passes in optimized and AddressSanitizer/UndefinedBehaviorSanitizer builds. The server adapter compiles with `-Wall -Wextra -Wpedantic -Wconversion -Werror` (excluding SDK unused-parameter warnings). Installed examples pass on scsynth at 44.1/48/96 kHz and supernova at 48 kHz; SCDoc indexing/rendering is clean. The installed numerical API check also verifies separate AM magnitudes, phase advances and RhythmPattern global normalization. Exact integration scope is recorded in `implementation-log.md`.

`benchmarks/modulation_cpu.cpp` measures the direct projection plus shaping for 200 updates. One local optimized run gave the following milliseconds; these are measured costs on the test host, not portable real-time guarantees:

| Bands × bins × frames | Mean ms/update | Maximum ms/update |
| --- | ---: | ---: |
| 2 × 16 × 400 | 0.014 | 0.016 |
| 24 × 16 × 400 | 0.165 | 0.221 |
| 32 × 32 × 512 (work limit) | 0.526 | 0.661 |

The musical examples use the supplied audio sample, a short 1.5-second history, and audible feature-controlled synthesis/filtering. Dataset classification evaluation and perceptual validation of a canonical phon/sone frontend remain separate research; they are not prerequisites hidden behind the delivered MIR2 adaptation.
