# MIR2 full roadmap status

The broader classical implementation now contains **135 public classes**, with
native DSP, SuperCollider APIs and an audible analysis-driven example for every
class. Work is tracked in local Git only. Nothing has been pushed or published.
The original classical integration was verified on 2026-10-24. The native
research extension and its verification are recorded in `implementation-log.md`.
Implementation coverage is distinct from general recognition accuracy on music.

| Family | Implemented scope | Evidence and remaining limits |
| --- | --- | --- |
| Original core | 11 spectral, cepstral, onset and YIN/MPM interfaces | Shared numerical and installed-server checks |
| Onset evidence | 4 additional evidence classes; all seven classical methods in Onsets2 | Astra High review, corrected formulas/frontend/peak picker, native/old/revised synthetic comparison; representative recordings deferred by user |
| Extended spectral and auditory | 22 spectral and 7 auditory classes | Independent mathematical checks; loudness is an informal MIR model, not ISO conformance |
| PNCC | Paper-based noise suppression, online power normalization, cepstra | Independent Python stage oracle and impulse-response frontend oracle; recognition accuracy unclaimed |
| Advanced pitch | 13 public classes | Candidate, SWIPE, probability, multipitch and causal contour checks; corpus accuracy remains a separate evaluation |
| Temporal, stereo, dynamics | 14 temporal, 7 stereo, 4 feature-dynamics classes | Event, moment, coherence and regression checks |
| Wavelets | 7 classes; Haar/db1–db8, periodic and redundant symmetric boundaries | Independent coefficients and finite-matrix oracles, energy identities, endpoint and streaming reconstruction checks |
| Constant-Q | CQT, VQT, CQCC, ChromaCQT | Direct-correlation/folding/resampling oracles at three rates; limited synthetic chroma comparison |
| Tonal/harmonic | 14 tonal and 11 harmonic classes; optional nine-quality chords | Pitch-class/key rotations, 108 extended templates, symmetry/transition checks, analytic harmonic fixtures |
| Rhythm | 16 classes, including multiband RhythmTransform and RhythmPattern | ACF/Fourier and multiband DFT oracles, pulse/tempo-change/AM fixtures and beat statistics |

`FundamentalCandidates` is the resolved public name for the earlier
`PitchCandidates` entry; `Vibrato2` resolves the earlier `Vibrato` name.
The shared implementation does not create redundant aliases for these names.
The project guide lists every public class, and the two original plans now mark
the implemented class checklists. Generic acceptance checklists in those plans
remain templates; they are not claims of identical corpus/performance coverage
for every class. See `implementation-log.md` for exact checks and limits.

## Optional research decisions

- **ChromaCQT — included:** synthetic comparisons establish useful low-register
  resolution in specific cases, with longer analysis delay and counterexamples
  where HPCP is better. The native unit and reproducible 400-row comparison are
  documented in [the evaluation](chroma-cqt-evaluation.md).
- **RhythmTransform / RhythmPattern — included:** bounded per-band modulation
  matrices have explicit normalization, phase, time pooling and latency, verified
  against a direct DFT. RhythmPattern is a declared Bark/log-power adaptation,
  with differences from the canonical descriptor. See
  [the modulation report](rhythm-modulation-implementation.md).
- **Wavelet/chord extensions — included:** db5–db8 and reflected symmetric
  boundaries add demonstrated polynomial/endpoint behavior. Nine chord qualities
  retain explicit symmetric-label ambiguities. Existing defaults remain; Symlet,
  Coiflet, critically sampled symmetric transforms and inversion-aware chord
  inference are outside this evaluated extension.
- **Neural pitch — audit complete, integration gate closed:** CREPE, PESTO,
  RMVPE, FCPE and SPICE were reviewed at pinned sources. PESTO v2 is selected
  for a possible separate prototype. Weight-license scope, frontend/decoder
  parity and bounded native runtime must be established before integration.
  Author timing is identified separately from unmeasured local performance.
  See [the full audit](neural-pitch-research.md). No neural runtime or weights
  are included.

## Remaining evaluation and release work

- **Recording-based quality:** the user explicitly deferred the proposed
  Onsets2 problem-recording test on 2026-10-23. General pitch/key/chord/beat
  accuracy, listening approval and broader corpus calibration are also not
  established by numerical fixtures or audible-example checks.
- **Release qualification:** hosted CI, native Windows/Linux/macOS-arm64
  execution and wider performance/endurance testing remain before publication.
  Local strict builds and a Windows cross-build do not substitute for those.

These are visible remaining evaluation/release items. No mandatory classical
family is relabeled as optional to hide an unfinished implementation.
