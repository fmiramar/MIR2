# PNCC implementation and reference gate

The initial PNCC gate required a clear paper-based implementation and a test
oracle. This expansion supplies both: the stages of Kim and Stern (2016),
[DOI 10.1109/TASLP.2016.2545928](https://doi.org/10.1109/TASLP.2016.2545928),
and an independently coded offline Python evaluator. No CMU implementation
source, training corpus, or pretrained parameters are copied or embedded.

## Processing conventions

- Audio is pre-emphasized, framed with a symmetric Hamming window, and padded
  to the next power of two at least twice the frame length.
- ERB-spaced centers use a sampled fourth-order analytic Hohmann gammatone
  response. The squared responses are normalized to unit discrete sum;
  amplitude tails below 0.005 of the peak are omitted.
- Five frames supply medium-time power; the original center frame is delayed
  two hops so it remains aligned with the environmental estimate. Startup
  uses zeros at negative time and waits for three frames before output.
  The five-frame span is `frameTime + 4*hopTime`; changing the hop changes
  this span even though the exponential forgetting factors are rescaled.
- Lower-envelope estimation, rectification, second lower-envelope flooring,
  excitation selection, temporal masking, and frequency smoothing implement
  the paper's equations 3–14. The baseline forgetting factors are 0.999/0.5
  for asymmetric following, 0.85 for temporal peaks, and 0.2 for masked power;
  the excitation boundary is twice the estimated background.
- Mean-power normalization and the 1/15 power law precede an orthonormal
  DCT-II. Time-dependent forgetting factors are exponentiated by
  `actualHopSeconds / 0.01` to preserve their physical time scales.

The analytic Hohmann response differs from the paper's Slaney real-filter
frontend. Mean power initializes at the first positive processed frame rather
than from a corpus-wide training mean. The normalization multiplier is one.
No utterance-level cepstral mean subtraction is imposed. These are documented
online adaptation choices, not numerical equivalence or speech-recognition
performance claims. Long silences allow the mean reference to decay; users
building a recognition frontend should evaluate an appropriate activity/reset
policy for their corpus.

The Hohmann bandwidth convention is `ERB = 24.7 + centerHz/9.265` and
`radius = exp(-2*pi*ERB/(sampleRate*(5*pi/16)))`. The sampled power transfer
is the fourth power of the one-stage squared magnitude. This matches the
four-stage analytic construction and its bandwidth conversion in Hohmann
(2002), as documented by the
[Auditory Modeling Toolbox implementation](https://amtoolbox.org/amt-1.6.0/doc/modelstages/hohmann2002_filter_code.php).
PNCC then applies its own discrete area normalization, which cancels an
otherwise arbitrary per-filter gain. The sampled matrix includes DC and
Nyquist, while the paper's finite sum excludes Nyquist; the requested upper
centre is also limited to 0.45 of the sample rate. These are further frontend
conventions to retain when reproducing MIR2 output.

## Independent oracle

`tests/python/pncc_reference.py` uses direct offline time-neighborhood sums
rather than the C++ ring buffer, and separate list equations for the nonlinear
stages. It generates `tests/reference/pncc-stages.csv` from deterministic band
powers containing silence, stationary backgrounds, band-limited entrances,
nonstationary modulation, gain changes and renewed activity after silence.
`MIR2PNCCTests.cpp` compares every recorded medium-power, background, floor,
weighted-power, mean-power and cepstral value to the C++ result. The fixture
contains 158 emitted frames and is reproducible without third-party packages.
Gammatone weights additionally have finite/nonnegative/unit-area checks over
44.1/48/96 kHz and FFT sizes 1024/2048/4096/8192.

An independent review checked the nonlinear stages against the paper's
equations 3–16 and added tests beyond the fixture: nonzero startup checks the
0.9-times-background initialization, hop changes check the forgetting-factor
time scaling, and a common input-power multiplier checks normalization
invariance. A separate frontend oracle constructs the closed-form impulse
response of four cascaded complex poles, Fourier transforms it, and compares
the normalized/truncated sampled weights. It covers eight centre frequencies,
including DC, at 44.1/48/96 kHz with absolute error below `1e-10`. The original
158-row Python fixture still regenerates byte for byte. The review found no
change needed to the established PNCC stage equations; it replaced a
cancellation-prone frequency-response denominator with its stable algebraic
equivalent and made server control-to-integer conversions bounded.

This stage-level oracle establishes equation and initialization agreement; it
does not validate speech recognition, model calibration, or audio equivalence
with the authors' frontend. The installed audio example and full-library NRT
checks exercise the native adapter separately.

## Resource bounds

Frame length is capped at 8192 samples, filter count at 128, and output count
at 64. All matrices and audio history are allocated in construction. Processing
has bounded FFT, matrix, five-frame and frequency-neighborhood loops, with no
callback allocation. First-result delay is the window plus two hops; the server
can expose an event at the next control block. Large filter/FFT configurations
need a larger SuperCollider realtime-memory pool.
