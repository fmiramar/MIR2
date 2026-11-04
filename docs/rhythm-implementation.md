# Causal rhythm implementation

The fourteen public classes share independent, bounded native kernels.
`Tempogram` uses positive mean-centered overlap-normalized autocorrelation;
`FourierTempogram` uses a mean-centered Hann-windowed Fourier projection at each
linear BPM grid point. The periodicity-analysis reference is
[Grosche, Müller and Kurth (2010)](https://resources.mpi-inf.mpg.de/MIR/tempogramtoolbox/2010_GroscheMuellerKurth_TempogramCyclic_ICASSP.pdf).
These are trailing absolute-tempo representations; MIRa does not implement the
paper's octave-folded cyclic representation or copy its software.

## Evidence and clock

The adapter max-pools control-rate novelty into approximately 100 Hz feature
frames. Pool duration and analysis hop are rounded to whole available frames;
the actual rate is used for lag, Fourier phase and smoothing calculations.
The configurable trailing history is 0.5–16 seconds with 8–512 linear BPM bins.
Each BPM candidate needs two complete periods of evidence. The FFT-based
correlation is zero-padded to avoid circular overlap. Fourier magnitudes divide
by the windowed absolute sum when normalization is enabled.

Tempo selection multiplies normalized correlation and Fourier support, then
weights by log-tempo distance from the previous estimate (or a weaker initial
prior). An explicit penalty resists half/double-tempo switches. `Tempo` returns
BPM and heuristic confidence. `BeatStatistics2` also exposes a secondary tempo,
dominance and normalized distribution entropy. `PulseClarity` is explicitly
`clip(2*maxEvidence*(1-entropy),0,1)`. `BeatHistogram` exponentially integrates
the combined evidence and optionally normalizes its sum to one.

`BeatTrack3` and `AutoTrack2` use the same forward clock. Tempo changes are
exponentially smoothed; phase correction is bounded and decays so phase cannot
reverse. Beat and half-beat impulses last one control block. The FFT constructor
uses Onsets2 novelty; `krOnsets` accepts an external control-rate envelope.
Neither is a wrapper around native BeatTrack nor a hidden machine-learning
model. Confidence is not a calibrated probability. Sparse events, syncopation,
compound meter and changing instrumentation can select the wrong pulse level.
Silence inhibits unlocked beat output after two periods (at least one second)
and eventually clears the trailing evidence. Lock holds an acquired prediction
through silence. There is no future lookahead, but acquisition and tempo changes
require history; the default warmup is two seconds and slow candidates need more.

## Statistics and beat-synchronous levels

`OnsetRate` and `OnsetStatistics2` retain rising-edge timestamps in a trailing
window. Density divides event count by the full requested window, including
during startup. Interval mean, population standard deviation and coefficient
of variation use adjacent retained events only. Event history is allocated once.

`TempoStability` returns `exp(-12*weightedStdDev(log2(BPM)))`, with confidence
weights and a minimum evidence threshold. It requires two accepted samples.
`TempoRubato` returns smoothed `100*d(log(BPM))/dt`, approximately percent change
per second. Missing tempo breaks the derivative chain to avoid an artificial
jump on reacquisition.

`BeatLoudness` outputs linear RMS and peak of the completed beat interval. It
is not LUFS or perceptual loudness. `BeatBandRatio` divides accumulated per-band
FFT power by whole-spectrum power over the completed interval. Boundary bins
are counted once; uncovered frequencies make the sum less than one. The first
beat arms accumulation, the next reports the interval. A missing beat longer
than `maxInterval` clears results. Beat boundaries have control-block precision;
spectral frames are assigned on arrival and retain their analysis latency.

## Verification and cost

`MIRaRhythmTests.cpp` checks correlation and Fourier values against independent
direct sums; pulse trains at 30/45/60/90/120/180/240/300 BPM; missing pulses;
weak subdivisions; a 110-to-140 BPM change; silence expiry; timing within 65 ms
on the deterministic clock fixture; onset interval, tempo and beat-energy
statistics. This establishes numerical/fixture behavior, not accuracy on an
annotated musical corpus. An optimized local 6-second, 100-Hz, 271-bin analysis
measured about 0.71 ms per analysis, excluding the server frontend. At a 0.1-second
hop this is approximately 0.71% of one core. Longer histories and denser grids
increase work; no realtime deadline is promised. Callbacks allocate no memory.

Each class has a synchronized looping-sample example. The source is gated into
repeatable attacks, and the analyzed evidence changes synthesis, filtering or
panning. These examples are included in the installed graph/audio checks.

## Later rhythm representation gate

The roadmap explicitly lists `RhythmTransform` and `RhythmPattern` under
“Evaluate later.” They remain research extensions, not aliases for this
single-envelope tempogram: a meaningful additional implementation would need
specified per-band modulation features, normalization, output layout, memory
and latency budgets, and an independent reference. The current scalar-envelope
engine does not justify claiming those separate representations are implemented.
No such class names or placeholder results are exported. Their gate remains
open in the roadmap rather than being counted as completed code.
