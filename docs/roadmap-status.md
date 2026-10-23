# MIR2 full roadmap status

The user's target is the complete broader classical library, with local Git
tracking, no publication, corrected Onsets2, and an audible example for every
public UGen. This is a work checkpoint, not a declaration of full completion.

| Family | Current state | Remaining work |
| --- | --- | --- |
| Initial spectral and YIN/MPM core | Existing implementation; audible examples revised | Recheck installed examples with expansion |
| Onsets and phase/complex/MKL evidence | Integrated rewrite, all seven classical methods; synthetic native/baseline comparison | Installed checks; annotated recorded-material quality corpus |
| Extended spectral and auditory | 29 classes integrated | Installed server and documentation checks; PNCC independent oracle gate |
| Advanced pitch | 13 public classes integrated, independent kernel checks | Installed checks and broader recorded-audio evaluation |
| Temporal/stereo/feature dynamics | 25 classes integrated; isolated class/example/doc checks passed | Combined installed server checks |
| Wavelets | DSP/header and plugin adapter drafted | Classes, documentation, reconstruction tests, integration |
| CQT/VQT/CQCC | Architecture discussed; implementation pending | Sparse kernels, reference checks, classes, examples, CPU measurement |
| Tonal/harmonic | Pending | Shared HPCP, key/chord, harmonic descriptors, examples and verification |
| Rhythm | Pending | Tempograms, tempo/phase tracking, statistics, examples and verification |
| Neural methods | Optional research gate | Model/runtime/license and realtime feasibility evaluation |

Parallel agents stopped at a usage limit. Their files are preserved. The parent
agent continues integration and records verified results in implementation-log.md.
Optional ChromaCQT, PNCC, broader chord vocabulary, extra wavelet families, and
later rhythm transforms must be evaluated against their explicit roadmap gates;
missing mandatory families must not be renamed as optional deferrals.
