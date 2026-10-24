# MIR2 implementation log

Date: 2026-08-20
Target: 0.1.0 unified first sprint

Both agentic plans were read in full before implementation. Their overlapping
`Pitch2`, frame, autocorrelation, real-time, documentation, and test policies
were resolved as one standalone `03-jumble-science/MIR2` project. All project
writes remained inside that directory while other agents worked elsewhere in
`03-jumble-science`.

## Implemented scope

Shared kernels:

- radix-2 FFT and zero-padded autocorrelation;
- spectral centroid, flatness, percentile, and flux formulas;
- Slaney/HTK Mel mappings, triangular banks, and DCT-II;
- fixed trailing frame/ring-buffer scheduling;
- difference, CMNDF, NSDF, local extrema, parabolic refinement, and octave
  relation analysis;
- deterministic energy/periodicity/zero-crossing voicing evidence.

Public version-0.1 analyzers:

- `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, `MFCC2`, `FFTFlux2`, and
  `Onsets2`;
- `PitchYIN`, `PitchMPM`, `Periodicity`, `Voicing`, and `Pitch2`.

This is the union of MIR2's stated first public milestone and Modern Pitch
Analysis's recommended first agent sprint. No future class name is installed
as a placeholder.

## Integration findings

Installed-server tests caught two SuperCollider-specific issues before
handoff. First, a control-rate UGen's calculation-rate sample rate is not the
audio input sample rate, so every analysis conversion now uses the world's
full audio rate. Second, a control-rate analyzer receiving an audio-rate input
is called once per server block but must explicitly consume the full audio
input block; the frame feeder now uses the full-rate buffer length.

Integration tests use dedicated ports 57145 through 57148 to avoid interfering
with other concurrent SuperCollider work.

## Real-time audit

Persistent pitch, FFT-flux, and onset storage is allocated with `RTAlloc` only
from constructors and freed only from destructors. Calculation callbacks do
not allocate, free, print, open files, load models, or acquire project-owned
locks. All algorithmic loops are bounded by literal frame, FFT, band, or output
counts validated at SynthDef construction.

FFT-chain access uses SuperCollider's required buffer synchronization. With
the documented `LocalBuf` pattern it performs no mutex acquisition; a non-local
shared FFT buffer may require the server's buffer lock.

## Verification

The final source state passed:

- strict AppleClang scsynth and supernova builds;
- strict MinGW Windows x86-64 cross-build;
- AddressSanitizer and UndefinedBehaviorSanitizer reference tests;
- C++ FFT inversion, spectral formulas, filter-bank/DCT, 44.1/48/96 kHz pitch,
  256-through-8192 frame, missing-fundamental, silence, noise, finite-guard,
  confidence, and octave-ambiguity tests;
- installed class compilation and exact class-origin collision checks;
- compilation of all test scripts and all 12 parenthesized help examples;
- forced SCDoc indexing and rendering of 11 class pages plus three guides with
  no warning or error;
- an installed scsynth smoke test covering all public analyzers;
- installed silence, DC, impulse, noise, and legal-extreme-parameter safety;
- an installed supernova smoke test;
- a nine-case NRT matrix at 44.1, 48, and 96 kHz with block sizes 1, 64, and
  256. YIN, MPM, `Pitch2`, periodicity, and voicing stayed inside their
  deterministic sine-test bounds in every case.

The local 48 kHz/64-sample-block benchmark measured approximate incremental
server CPU per instance of 0.17 for the four-descriptor spectral core, 0.19
for 13-coefficient `MFCC2`, 0.17 for SuperFlux-style `Onsets2`, 1.75 for
2048-sample YIN or MPM, and 4.91 for the combined `Pitch2` + `Periodicity` +
`Voicing` family. These are local comparative figures, not portable targets.

Both scsynth and supernova variants were installed into the local
SuperCollider user extension directory. Hosted GitHub jobs and publication
were not run.

## Explicitly deferred

- probabilistic candidates, pYIN tracking, SWIPE', consensus, and pitch
  salience;
- multipitch, predominant melody, and contour tracking;
- broader spectral, auditory/cepstral, tonal, rhythm, temporal, stereo,
  wavelet, CQT, and VQT families;
- all neural inference and model packaging.

These remain roadmap phases requiring their own paper-based implementation,
tests, CPU evaluation, and any applicable code/model/data license review.


## 2026-10-22 development expansion checkpoint

The broader implementation remains active. The expansion currently contains
82 integrated public classes in the onset, extended spectral, auditory,
advanced pitch, temporal, stereo, and feature-dynamics families, plus seven
newly completed wavelet classes awaiting the next installed example sweep.

Verified at this checkpoint:

- strict scsynth and supernova compilation for the 82-class integration;
- six CTest executables passed, with assertions enabled even in Release;
- installed class compilation and exact class-origin checks;
- forced installed SCDoc indexing and rendering of all 82 class pages with no
  warning or error;
- construction of all 82 audible example SynthDefs and three-second NRT
  execution of each; every output was finite, bounded and non-silent;
- wavelet strict scsynth build plus exact Haar oracle, random db1–db4
  reconstruction and energy conservation across frame sizes 32–8192 and all
  legal levels.

The Onsets2 synthetic report includes independently swept thresholds for
revised, original MIR2, and external native OnsetsDS implementations. It does
not claim listening approval or accuracy on annotated recordings. The shared
FFT reader no longer changes the input buffer's coordinate representation.
A review also corrected the old/new pitch constructor calls to
ClearUnitOutputs: its second argument is a sample count, not an output count.

Local installation was performed. No Git push, hosted CI, release upload, or
publication occurred. Remaining mandatory families are tracked explicitly in
roadmap-status.md. Parallel agents hit a usage limit; their work was preserved
and integration continued in the parent agent.

## 2026-10-24 — broader classical integration verified

The development catalog now has 132 public classes. This completes the primary
classical families in the MIR2 and Modern Pitch plans, including seven wavelets,
sparse CQT/VQT/CQCC, PNCC, 25 tonal/harmonic analyzers and fourteen rhythm
analyzers. Both plans mark implemented class entries and preserve explicit
optional research gates. `roadmap-status.md` distinguishes implementation from
recording accuracy and release qualification.

Final integration corrected unsafe float-to-int conversions, CQCC interpolation
in Hz, wavelet flux after silence, and example arithmetic that accidentally
requested ultrasonic filter frequencies under SuperCollider's left-to-right
binary-operator rules. Every class now has an audible, synchronized example.
The onset report and CSV were regenerated from the reviewed implementation.
At threshold 0.3, revised SuperFlux has 120 true positives, 24 false positives
and no misses on the deterministic fixture matrix; the prior implementation
has 1371 false positives on that same matrix. These are fixture diagnostics,
not held-out recording accuracy.

Verification completed locally against SuperCollider 3.14.1:

| Check | Result |
| --- | --- |
| Strict macOS x86_64 scsynth and supernova builds | Passed |
| Strict Windows x86_64 MinGW cross-build | Passed; native Windows execution not performed |
| Numerical CTest suite | 13/13 passed, 27.60 seconds in the recorded full run |
| Address/undefined-behavior sanitizers | 13/13 passed, 260.23 seconds |
| PNCC stage fixture regeneration | Byte-for-byte match; standard-library Python oracle |
| Updated PNCC frontend/hop/startup/gain checks | Passed |
| Installed class-origin/collision check | All 132 public names resolve to MIR2 |
| Forced SCDoc indexing and rendering | All 132 class pages and three existing guides, without SCDoc warnings/errors |
| scsynth audible examples, 44.1 kHz / block 32 | 132/132 passed |
| scsynth audible examples, 48 kHz / block 64 | 132/132 passed |
| scsynth audible examples, 96 kHz / block 128 | 132/132 passed |
| supernova audible examples, 48 kHz / block 64 | 132/132 passed |
| Silent playback inputs, 48 kHz / block 64 | 132/132 finite/stable |
| Existing focused SC verification script compilation | Passed |

Audio checks render four seconds per class and require finite, bounded,
non-silent audio. A dedicated third channel checks native analyzer outputs
before the audible mappings for NaN/Inf. Silent-playback tests substitute
DC.ar(0), preserving rate. They do not require synthesized demonstration voices
to be silent. The final 48-kHz and silent-input runs cover the complete final
catalog; after localized documentation corrections, the affected thirteen
examples were rerun at 44.1/96 kHz and supernova and replaced their earlier
results. All final per-class results pass. These are construction/stability
checks, not human listening approval or long-term realtime stress tests.

The reusable generator now batches language graphs to respect the selector
limit and assigns a unique NRT score path to each run. SuperCollider's default
temporary score names can collide across separate sclang processes. That
harness issue was corrected before the final full audible/silent runs.

Both server variants are installed in the SuperCollider user Extensions tree.
Generated binaries, scores, WAV files and detailed logs remain ignored local
build output. Existing unrelated VSTPlugin Objective-C duplicate-class messages
can appear when the global extension tree is scanned; no MIR2 class collision,
SCDoc warning or analyzer invalid-value failure remains. No unrelated extension
was modified. No files were pushed or published.

The user-deferred Onsets2 representative-recording evaluation remains deferred.
Optional ChromaCQT, later rhythm representations, expanded wavelet/chord choices,
neural inference, hosted/native-platform CI and broader corpus/performance
qualification remain visible in `roadmap-status.md`.
