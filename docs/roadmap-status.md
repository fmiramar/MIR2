# MIR2 full roadmap status

The broader classical implementation now contains **132 public classes**, with
native DSP, SuperCollider APIs and an audible analysis-driven example for every
class. Work is tracked in local Git only. Nothing has been pushed or published.
Final integration verification was completed on 2026-10-24.
Implementation coverage is distinct from general recognition accuracy on music.

| Family | Implemented scope | Evidence and remaining limits |
| --- | --- | --- |
| Original core | 11 spectral, cepstral, onset and YIN/MPM interfaces | Shared numerical and installed-server checks |
| Onset evidence | 4 additional evidence classes; all seven classical methods in Onsets2 | Astra High review, corrected formulas/frontend/peak picker, native/old/revised synthetic comparison; representative recordings deferred by user |
| Extended spectral and auditory | 22 spectral and 7 auditory classes | Independent mathematical checks; loudness is an informal MIR model, not ISO conformance |
| PNCC | Paper-based noise suppression, online power normalization, cepstra | Independent Python stage oracle and impulse-response frontend oracle; recognition accuracy unclaimed |
| Advanced pitch | 13 public classes | Candidate, SWIPE, probability, multipitch and causal contour checks; corpus accuracy remains a separate evaluation |
| Temporal, stereo, dynamics | 14 temporal, 7 stereo, 4 feature-dynamics classes | Event, moment, coherence and regression checks |
| Wavelets | 7 classes; Haar/db1–db4 periodic analysis/reconstruction | All supported orders/levels, energy identities, streaming reconstruction and sanitizer checks |
| Constant-Q | CQT, VQT, CQCC | Direct complex-correlation and linear-Hz resampling oracles at three rates |
| Tonal/harmonic | 14 tonal and 11 harmonic classes | Pitch-class rotations, 72 key-profile cases, 24 triads, analytic geometry and harmonic fixtures |
| Rhythm | 14 classes, including AutoTrack2 and BeatTrack3 | Direct ACF/Fourier oracles, pulse/tempo-change/missing-beat fixtures and beat statistics |

`FundamentalCandidates` is the resolved public name for the earlier
`PitchCandidates` entry; `Vibrato2` resolves the earlier `Vibrato` name.
The shared implementation does not create redundant aliases for these names.
The project guide lists every public class, and the two original plans now mark
the implemented class checklists. Generic acceptance checklists in those plans
remain templates; they are not claims of identical corpus/performance coverage
for every class. See `implementation-log.md` for exact checks and limits.

## Explicit later gates

- **ChromaCQT:** the roadmap requires measured benefit over HPCP/chromagram,
  not another name for octave folding. The CQT engine is available, but that
  comparative benefit has not been established. See the constant-Q report.
- **RhythmTransform / RhythmPattern:** explicitly listed as “Evaluate later.”
  A separate per-band modulation representation needs specified normalization,
  dimensions, latency and an independent reference. The single-envelope
  tempogram is not presented as either of these missing representations.
- **Additional wavelet families/boundaries and chord vocabularies:** supported
  choices are explicit; unsupported alternatives are rejected or documented,
  rather than mapped silently to the initial basis or major/minor vocabulary.
- **Neural pitch:** optional model/runtime/license and realtime feasibility
  research. No model weights, inference runtime, placeholders or neural
  performance claims are included in this native classical library.
- **Recording-based quality:** the user explicitly deferred the proposed
  Onsets2 problem-recording test on 2026-10-23. General pitch/key/chord/beat
  accuracy, listening approval and broader corpus calibration are also not
  established by numerical fixtures or audible-example checks.
- **Release qualification:** hosted CI, native Windows/Linux/macOS-arm64
  execution and wider performance/endurance testing remain before publication.
  Local strict builds and a Windows cross-build do not substitute for those.

These are visible remaining evaluation/release items. No mandatory classical
family is relabeled as optional to hide an unfinished implementation.
