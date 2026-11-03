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

## 2026-10-24 — optional research extension integrated

The catalog now contains 135 public classes. Three native analyzers were added:
ChromaCQT, RhythmTransform and RhythmPattern. The CQT gate includes a reproducible
400-row synthetic comparison showing specific low-register benefits, longer
window delays and cases where HPCP is better. Rhythm modulation preserves the
association between spectral bands and modulation rates; RhythmPattern clearly
documents its Bark/log-power differences from the canonical psychoacoustic
descriptor. Independent correlation/DFT oracles verify the numerical definitions.

The seven wavelet classes now support db1–db8 and a redundant reflected symmetric
mode. Independent spectral factorization, dense finite matrices, energy and
streaming reconstruction checks cover the extension. ChordTrack/ChordStrength
and ChordStats support nine qualities and 109 states, with all 108 pitch-class
templates checked and symmetric-label ambiguities documented. Default periodic
wavelets and major/minor chords remain available, including previous SynthDefs.

Verification completed against the installed SuperCollider 3.14.1 extension:

| Check | Result |
| --- | --- |
| Strict macOS x86_64 scsynth and supernova builds | Passed without compiler warnings |
| Strict Windows x86_64 MinGW cross-build | Passed; native Windows execution remains unperformed |
| Full C++ numerical suite | 16/16 passed, 30.08 seconds |
| ASan/UBSan for affected kernels and related CQT/tonal code | 6/6 passed, 31.76 seconds |
| Installed class-origin check | All 135 public classes resolve to MIR2 |
| Forced SCDoc indexing and rendering | 135 class pages and three guides; no final SCDoc warnings/errors |
| All audible examples at 48 kHz / block 64 | 135/135 passed |
| Thirteen new/changed examples at 44.1 kHz / block 32 and 96 kHz / block 128 | 13/13 passed at each rate |
| Thirteen new/changed examples on supernova at 48 kHz / block 64 | 13/13 passed |
| Thirteen new/changed examples with silent playback | 13/13 finite/stable |
| Installed numerical API checks at all three rates and on supernova | Passed |

The installed numerical script checks known chroma concentration, exact silence,
normalizations, two modulation amplitudes and their phase advances, the extended
Cmaj7 label, 109-state chord entropy, and exact 1024-sample wavelet reconstruction
delay. Both current symmetric/db8 and previous periodic/db4 SynthDef input layouts
match the separately recorded delayed source exactly in these float WAV fixtures.
Maximum observed phase-advance error is below 0.000136 radians. The check samples
K2A at block boundaries so interpolation is not mistaken for analyzer updates.
The two initial RhythmPattern argument-documentation warnings were corrected
before the final clean SCDoc run. No DSP changes were required by the server runs.

The neural audit covers pinned CREPE, PESTO, RMVPE, FCPE and SPICE sources,
separate code/weight terms, training data, frontends, context and deployment.
PESTO v2 is the first candidate for a separate prototype. Its integration gate
remains closed pending established checkpoint-license scope, frontend/decoder
parity and bounded runtime measurements. No neural weights/runtime are included,
and author-reported performance is not presented as a local measurement.

Both server variants and the final docs are installed in the user Extensions
tree. Generated builds, scores, WAVs and detailed logs remain ignored. These
checks establish mathematical and server behavior, not human listening approval
or general recognition accuracy. The user-deferred Onsets2 recording test and
release-platform/endurance qualification remain later work. Git tracking is
local; nothing has been pushed or published.

## 2026-10-26 — optional native PESTO extension

Completed `neural/` as a separate MIR2Neural build with the `PitchPESTO` UGen,
explicit pinned model export, native CPU frontend/network/confidence/decoder,
and a deterministic one-hop computation pipeline. The base 135 classes and
default build remain independent. No weights, Python framework or inference
runtime are bundled; exported model Buffers are explicit local assets.

The reference comparison covers 1,800 frames at 16/44.1/48/96 kHz. Maximum pitch
difference is 0.05150 cents. Scheduled processing is bit-identical to the full
native pass at 32/64/128-sample blocks. Initial timing failures led to weighted
frontend scheduling and vectorized, incremental Toeplitz output processing.
The final single-instance sweep records one observed deadline overrun in 41,300
blocks, at 96 kHz/block 64; block 128 is recommended at that rate. Neither that
sweep nor the source audit is presented as a production real-time guarantee.

Strict macOS x64 scsynth/supernova and Windows x64 cross-builds pass. Three
native and three ASan/UBSan CTests pass, as does the real-model 96 kHz sanitized
streaming fixture. Installed contract tests verify frequency, confidence,
warmup, silence, malformed buffers and fail-silent model removal. The actual
audible help graph renders successfully at four sample rates and on supernova;
both channels are audible and all primitive outputs remain finite. Forced
SCDoc indexing and the changed class/guide render without warnings or errors.

Both server plugins and local model assets are installed. Source, tests and
CI definitions are tracked locally; no workflow, release or publication was
triggered. [The report](neural-pitch-implementation.md) and
[machine-readable measurements](neural-pitch-measurements.json) preserve hashes,
timing distributions, memory, latency, tolerances and remaining qualification.

### Follow-up live checks

Both server backends passed the muted 48 kHz/block-64 live harness: 60 seconds
of sampled phases per backend, two analyzers sharing a model, independent
silence handling, 800 valid analysis replies and 64 create/free cycles. Mean
reported single/two-instance CPU was 27.54/53.52% on scsynth and 27.48/32.69%
on supernova with a parallel group. Idle CPU returned near baseline after the
cycles. [The live measurements](neural-pitch-live-measurements.json) preserve
phase statistics and trace hashes; the report distinguishes these short local
checks from longer endurance and general polyphony qualification.

## 2026-10-28 — musical documentation overhaul, CC0 audio corpus and MIR2GUI live toolkit

Completed the comprehensive overhaul of all 135 base UGen help files and PitchPESTO
under the approved master plan (MIR2_DOCUMENTATION_AND_VISUALIZATION_PLAN.md).

1. Curated CC0 Audio Corpus (sounds/):
   Installed 7 clean, uncompressed CC0 1.0 Universal audio files (44.1 kHz 16-bit WAV,
   peak-normalized to -1.0 dBFS with 5 ms zero-crossing edge fades): drums.wav,
   piano-chords.wav, melody.wav, plucks.wav, stereo.wav, bass-melody-mix.wav,
   and texture.wav. Full provenance, Freesound IDs, author attribution, and DSP
   processing history are documented in sounds/README.md.
2. Pure-SuperCollider Live MIR GUI Toolkit (Classes/MIR2GUI.sc):
   Implemented 10 standard real-time MIR visualizers: MIR2Scope, MIR2BarView,
   MIR2Heatmap, MIR2ChromaWheel, MIR2TonnetzView, MIR2StereoScope,
   MIR2PianoRoll, MIR2RhythmWheel, MIR2TristimulusView, and MIR2EnvelopeView.
   All views are integrated via factory methods (MIR2GUI.soundPath, MIR2GUI.chroma,
   etc.), execute safely across headless and graphical environments, and cleanly release
   their polling SkipJack routines upon window closure.
3. Musical Examples Across All 135 Classes:
   Replaced sterile speech-gain scaling with handcrafted, musically meaningful DSP processes
   (vocoders, resynthesis banks, adaptive combs, envelope shapers, harmonic resynthesizers,
   and dynamic stereo spatializers). Every example provides graceful fallback to self-contained
   synthetic source graphs and includes a commented SoundIn.ar(0) alternative.
4. End-to-End Verification:
   - compile_check.scd: Passed without errors (MIR2_COMPILE_OK).
   - compile_help_examples.scd: Compiled every executable snippet across all 135 help files (MIR2_HELP_EXAMPLES_OK).
   - verify_scdoc.scd: Cleanly indexed and rendered all 135 class help files and guides with 0 errors and 0 warnings (MIR2_SCDOC_OK).
   - generate_example_check.py --nrt: Headless NRT audio rendering across all 135 classes verified finite audible levels with zero badPrimitive calls.
   - All files mirrored to user Extensions directory (~/Library/Application Support/SuperCollider/Extensions/MIR2/).
