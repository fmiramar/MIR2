# Spectral and auditory implementation

This module implements the remaining mandatory scalar/subband spectral descriptors and the Bark, ERB, gammatone, BFCC, GFCC, LFCC, and Loudness2 families. These are native server implementations with bounded construction-time state; processing never allocates memory. The implementation is research-oriented rather than a binary compatibility wrapper for sc3-plugins or another MIR library.

## Public classes and conventions

The new classes are `FFTCrest2`, `FFTSpread2`, `FFTSlope2`, `FFTPower2`, `FFTPeak2`, `FFTSubbandFlatness2`, `FFTSubbandFlux2`, `FFTSubbandPower2`, `SpecEntropy`, `SpecDecrease`, `SpecComplexity`, `SpecContrast`, `SpecStrongPeak`, `SpecHFC`, `SpecBandwidth`, `SpecIrregularity`, `SpecVariation`, `SpecRMS`, `SpecEnergy`, `SpecBandEnergy`, `SpecBandRatio`, `SpecSparsity`, `BarkBands`, `ERBBands`, `GammatoneBands`, `BFCC`, `GFCC`, `LFCC`, and `Loudness2`.

The installed core and extension class sources were searched before adding these names; no conflicting definitions were found. `MIR2SpectralBands` and `MIR2AuditoryAnalyzer` are abstract language-side validation helpers, not server analyzers.

All spectral inputs are raw, one-sided FFT magnitudes. `spectrumMode=0` uses magnitude and `spectrumMode=1` uses its square. Unless a class explicitly says otherwise, there is no factor of two for interior bins, FFT-size division, or window-gain correction. Thus `FFTPower2`, `SpecEnergy`, and `SpecRMS` are descriptors of the FFT-bin vector and are not calibrated time-domain power/RMS meters. The only calibrated power-to-acoustic convention in this module is the explicitly parameterized `Loudness2` conversion.

Scalar ranges include bin centers between `minFreq` and `maxFreq`; zero maximum means Nyquist. Empty ranges and silence produce zero. Adjacent subbands are half-open except for the final included upper edge, preventing double counting at a boundary. Subband arrays are literal construction settings with at most 64 bands. All FFT readers retain their result when no new frame arrives and leave the source spectrum unchanged. Flux and variation return zero on their first frame.

## Exact spectral definitions

For selected nonnegative weights `w[j]`, frequencies `f[j]`, count `M`, sum `S`, centroid `c=sum(w*f)/S`, and norm `L2=sqrt(sum(w*w))`:

| Class | Definition |
| --- | --- |
| FFTCrest2 | `max(w) / (S/M)` |
| FFTSpread2 | `sqrt(sum(w*(f-c)^2)/S)` in Hz |
| FFTSlope2 | `sum((f-mean(f))*w) / (sum((f-mean(f))^2)*S)` |
| FFTPower2 | `S/M`, power by default |
| SpecRMS | `sqrt(sum(magnitude^2)/M)` |
| SpecEnergy / SpecBandEnergy | `S`; names distinguish full-spectrum and restricted-range uses |
| SpecBandRatio | selected `S` divided by sum over all one-sided bins |
| SpecEntropy | `-sum(p*log(p))`, `p=w/S`; optional division by `log(M)` or conversion to bits |
| SpecDecrease | `sum((w[j]-w[0])/j, j=1..M-1) / sum(w[j], j=1..M-1)` |
| SpecComplexity | count of local maxima above `threshold*max(w)`; weighted mode sums each peak's `w/max(w)` |
| SpecStrongPeak | `max(w)/S`, explicitly the maximum-bin concentration convention |
| SpecHFC | `sum(f*w)` or `sum(f*f*w)`, frequencies in Hz |
| SpecBandwidth | `(sum(w*abs(f-c)^p)/S)^(1/p)`, `0.25 <= p <= 8` |
| SpecIrregularity, Jensen | `sum((w[j]-w[j+1])^2)/sum(w^2)` over in-range adjacent pairs |
| SpecIrregularity, Krimphoff | `sum(abs(w[j]-(w[j-1]+w[j]+w[j+1])/3))` over interior bins |
| SpecVariation | `1-dot(w,previousW)/(norm(w)*norm(previousW))`; silent comparisons return zero |
| SpecSparsity | Hoyer: `(sqrt(M)-S/L2)/(sqrt(M)-1)`; one occupied bin yields one, silence yields zero |
| FFTSubbandFlatness2 | geometric/arithmetic mean of floored weights per band |
| FFTSubbandFlux2 | L2 frame difference per band, optionally rectified and normalized by current-band L2 norm |
| FFTSubbandPower2 | mean selected weights per band |
| SpecContrast | dB ratio of mean top and bottom `ceil(quantile*M)` weights; optional bottom-mean dB vector |

The irregularity definitions are applied to FFT bins rather than tracked harmonics; no unmentioned final zero bin is added to Jensen's numerator. `SpecStrongPeak` does not claim the width-based StrongPeak definition found in some other packages. HFC uses Hz, not FFT-bin indices. These conventions are deliberate and documented in the class help rather than silently borrowing numerically incompatible names.

`FFTPeak2` and the shared `extractSpectralPeaks` kernel detect maxima greater than their left and at least as large as their right neighbor. DC/Nyquist are excluded. Optional three-point log-parabolic interpolation estimates offset and peak magnitude. The caller supplies output storage; the strongest `capacity` peaks are retained then sorted by increasing frequency. The generic selection costs O(bins*capacity), plus O(capacity log capacity) sorting; FFTPeak2 uses capacity one. It is not a fundamental estimator.

## Auditory models

Bark bands use Traunmuller's uncorrected rational mapping `z=26.81*f/(1960+f)-0.53`. ERB bands use the Glasberg-Moore ERB rate `21.4*log10(1+0.00437*f)`. Linear bands use Hz directly. Each triangle is piecewise linear in Hz with its three points equally spaced on the chosen scale. Discrete area normalization divides by the sum of sampled bin weights. Unresolved filters produce zero, rather than pretending to gain frequency resolution. BFCC/LFCC apply natural log and the same DCT-II/lifter definitions as MFCC2.

GammatoneBands is a time-domain Hohmann fourth-order complex cascade. With `aGamma=pi*720/(64*36)`, `b=(24.7+center/9.265)/aGamma`, and `r=exp(-2*pi*b/sampleRate)`, every complex stage has pole `r*exp(i*2*pi*center/sampleRate)`. Its total analytic normalization is `2*(1-r)^4`. Each stage applies `(1-r)` internally for better conditioning. Output power is half the squared analytic envelope; a centered unit-peak sinusoid approaches 0.5. A one-pole envelope smoother and explicit hop are specified in seconds. Centers include the selected endpoints and are ERB-rate spaced. High centers are limited to 0.45 of sample rate.

GFCC applies cube-root RMS amplitude, equivalently smoothed power to the 1/6, or natural log power, followed by orthonormal DCT-II. The causal analytic-envelope/exponential-smoothing convention is stated openly: this is not a claim to reproduce every half-wave-rectified, decimated, or framed GFCC recipe. Cube-root silence is zero; log silence has a floor-derived c0.

### Loudness2 model and limits

The immediate source is Nick Collins's 2007 SuperCollider informal perceptual loudness implementation. The 42-band frequency grid and 42x11 contour table are retained from SuperCollider 3.14.1 `server/plugins/Loudness.cpp` under GPL-2.0-or-later, redistributed in this GPL-3.0-or-later project. The source copyright names James McCartney (2002) and the implementation names Nick Collins (2007). No other external source code or coefficient table was copied in this module.

Band edges are converted from the original 44.1 kHz/1024 bin grid to Hz. Fractional bin overlap supports other sample rates and FFT sizes. Parseval scaling uses one-sided symmetry, `fftSize^2`, and a declared average window power. `calibration=100` means a unit-peak sinusoid represents 100 dB SPL; this convention must match the user's physical input setup. The original per-bin spectral-mask heuristic is omitted.

Each band's SPL is interpolated through the inherited table. The table is used only within its documented numerical range, with below-range contributions zero and a per-band 100-phon ceiling. Total excitation is `sum(10^(bandPhon/10)-1)`, removing the native model's silent residual. Smoothed total phons are `10*log10(1+excitation)`. Above 40 phon, sones are `2^((phon-40)/10)`; below 40 they are `(phon/40)^2.86`, continuous at 40 and zero at silence. Attack/release time constants operate on excitation and use elapsed samples between FFT frames, not an assumed hop.

The source table does not document whether its contours assume a free or diffuse field. MIR2 makes no such claim and applies no acoustic-field correction; externally calibrated and equalized input plus independent validation are required for absolute acoustic uses. This model remains an informal MIR control feature, without ISO conformance or claims of modern perceptual accuracy. It is not an EBU R128 meter.

## PNCC reference gate

The initial paper/oracle gate has now been addressed in the separate PNCC module.
See [PNCC implementation and independent oracle](pncc-implementation.md) for
its exact equations, frontend differences, initialization, stage fixtures and
limitations. This is not a log-to-root shortcut or an imported recognition
library; all medium-time suppression and normalization stages are present.

## Validation and cost

`MIR2AdvancedSpectralTests.cpp` checks known-spectrum scalar formulas, entropy units, range behavior, cosine variation, contrast quantile means, strongest-peak selection, exact log-parabolic interpolation, silence, and nonfinite bins. `MIR2AuditoryTests.cpp` checks scale inverses, normalized filter responses, orthonormal DCT energy conservation, the independent binomial closed-form gammatone impulse response, center/off-band tones at 44.1/48/96 kHz, and 1 kHz loudness calibration across FFT sizes 1024/2048/4096.

An optimized standalone run measured approximately 80 microseconds for a group of all 16 scalar kernels on a 2048-point spectrum, and 0.055 CPU seconds for one second of 64-channel gammatone filtering at 48 kHz. These are local kernel timings, not cross-platform server guarantees; the tests print current timings when rerun. GFCC DCT and server overhead are additional. Gammatone filtering has frequency-dependent causal delay and envelope smoothing; it adds no lookahead. Spectral analyzers add no lookahead beyond their upstream FFT.

Per-class help includes a playable analysis-driven example with `ExampleFiles.child`, buffer synchronization, fade-in, and cleanup. Full SC class compilation, NRT integration, installed SCDoc indexing, and rendering are centralized with the rest of MIR2 and must pass before release. These standalone checks alone do not claim that every help example has passed server execution.

## Primary sources

- [Peeters (2004), A Large Set of Audio Features for Sound Description](https://recherche.ircam.fr/anasyn/peeters/ARTICLES/Peeters_2003_cuidadoaudiofeatures.pdf): descriptor taxonomy and common equations. [IRCAM-hosted copy](https://discussion.forum.ircam.fr/uploads/default/original/1X/ffa6a82f823873f864681994c271fb157e41a627.pdf).
- [Jiang et al. (2002), Music Type Classification by Spectral Contrast Feature](https://hcsi.cs.tsinghua.edu.cn/Paper/Paper02/200218.pdf): peak/valley quantiles. MIR2 omits the paper's learned KL transform and expresses ratios in dB.
- [Hoyer (2004), Non-negative Matrix Factorization with Sparseness Constraints](https://www.jmlr.org/papers/volume5/hoyer04a/hoyer04a.pdf): normalized L1/L2 sparsity.
- [Smith, Spectral Audio Signal Processing, Quadratic Interpolation of Spectral Peaks](https://www.dsprelated.com/freebooks/sasp/Quadratic_Interpolation_Spectral_Peaks.html): interpolation geometry.
- [Traunmuller (1990), Analytical expressions for the tonotopic sensory scale](https://doi.org/10.1121/1.399849).
- [Glasberg and Moore (1990), Derivation of auditory filter shapes from notched-noise data](https://doi.org/10.1016/0378-5955(90)90170-T).
- Hohmann (2002), Frequency analysis and synthesis using a Gammatone filterbank, Acta Acustica united with Acustica 88:433–442. [Author-origin AMT equations and implementation](https://amtoolbox.org/amt-1.6.0/doc/modelstages/hohmann2002_filter_code.php) were consulted for the equations; MIR2's implementation is independently written.
- [SuperCollider 3.14.1 Loudness.cpp](https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/Loudness.cpp): GPL-2.0-or-later contour table and immediate loudness model source.
- [Kim and Stern (2016), Power-Normalized Cepstral Coefficients for Robust Speech Recognition](https://www.cs.cmu.edu/~robust/Papers/KimStern16_proof.pdf): independently implemented PNCC stages, evaluated in the PNCC report.
