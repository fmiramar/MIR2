# Algorithm origins

MIR2 is an original clean-room implementation. Papers and upstream projects
are algorithmic, interface, or behavioral references; their implementations
are not runtime dependencies.

## Pitch

- Alain de Cheveigné and Hideki Kawahara, “YIN, a fundamental frequency
  estimator for speech and music,” *JASA* 111(4), 2002,
  DOI `10.1121/1.1458024`. MIR2 implements the difference function, cumulative
  mean normalized difference, threshold search, and parabolic refinement from
  the published algorithm.
- Philip McLeod and Geoff Wyvill, “A Smarter Way to Find Pitch,” *ICMC*, 2005.
  MIR2 implements normalized square difference, peak selection, clarity, and
  parabolic refinement from the paper.
- SuperCollider `Pitch`, sc3-plugins `Tartini`, and sc3-plugins `Qitch` were
  inspected for public interface, behavior, and class-name history. No source
  from those implementations is copied into MIR2.

## Onsets

- Sebastian Böck and Gerhard Widmer, “Maximum Filter Vibrato Suppression for
  Onset Detection,” *DAFx-13*, 2013. `Onsets2` uses the paper's core idea of
  positive log-spectral difference against a frequency-axis maximum-filtered
  previous frame, with an independently written causal threshold stage.
- Dan Stowell and Mark Plumbley, “Adaptive Whitening for Improved Real-time
  Audio Onset Detection,” *ICMC*, 2007. SuperCollider `Onsets` was inspected as
  a behavioral and migration reference, not copied.

## Cepstral and spectral analysis

- Steven B. Davis and Paul Mermelstein, “Comparison of Parametric
  Representations for Monosyllabic Word Recognition in Continuously Spoken
  Sentences,” *IEEE TASSP*, 1980. `MFCC2` follows the standard filter-bank,
  logarithm, and DCT pipeline with explicitly selectable conventions.
- Malcolm Slaney, *Auditory Toolbox*, version 2 technical report, 1998. The
  named Slaney Mel scale is implemented from its published piecewise mapping.
- `SpecCentroid2`, `SpecFlatness2`, `SpecPcile2`, and `FFTFlux2` use their
  standard mathematical definitions documented in their class help pages.

## External comparison policy

FluCoMa, librosa, and Essentia may be used externally for development
comparisons. MIR2 does not link or embed them. Essentia source is not copied or
translated because it is distributed under AGPL terms; librosa is likewise not
a runtime dependency.

