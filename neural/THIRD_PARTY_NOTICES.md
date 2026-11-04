# Third-party notices

## PESTO

The native network, confidence classifier and pitch decoder adapt the PESTO
implementation by Alain Riou / Sony CSL Paris at revision
`62bc0c9702558f19af4593752947fb9db1eadac9`:

- [Repository](https://github.com/SonyCSLParis/pesto/tree/62bc0c9702558f19af4593752947fb9db1eadac9).
- [Network and confidence classifier](https://github.com/SonyCSLParis/pesto/blob/62bc0c9702558f19af4593752947fb9db1eadac9/pesto/model.py).
- [Pitch decoder](https://github.com/SonyCSLParis/pesto/blob/62bc0c9702558f19af4593752947fb9db1eadac9/pesto/utils/reduce_activations.py).
- [Upstream license](https://github.com/SonyCSLParis/pesto/blob/62bc0c9702558f19af4593752947fb9db1eadac9/LICENSE.md): GNU Lesser General Public License, version 3.

The upstream license is retained verbatim in `licenses/LGPL-3.0.md`. The MIRa
adaptation is distributed under the parent project's GNU GPL license. Native
changes include fixed-shape C++ inference, explicit model-buffer validation,
compact frontend kernels, allocation-free inference state and work distributed
across audio blocks. No upstream Python runtime is linked into the plugin.

Technique: Alain Riou and colleagues, “PESTO: Real-Time Pitch Estimation with
Self-supervised Transposition-equivariant Objective,”
[arXiv:2508.01488](https://arxiv.org/html/2508.01488v1).

## Frontend provenance

PESTO's `pesto/utils/hcqt.py` identifies its constant/variable-Q kernel generator
as adapted from [nnAudio](https://github.com/KinWaiCheuk/nnAudio).
Its `pesto/utils/cached_conv.py` identifies cached-convolution code from
[Antoine Caillon / acids-ircam](https://github.com/acids-ircam/cached_conv).
The offline exporter uses these files from the externally supplied pinned PESTO
checkout. They are not copied into this extension. Exported kernels retain their
float32 coefficients and normalization; the native frontend performs the matching
cross-correlation over a trailing window.

## Checkpoint and generated assets

The only accepted source checkpoint is `pesto/weights/mir-1k_g7.ckpt` from the
pinned PESTO revision, with SHA-256:

```text
16c32e06ddd950e3e4866dfa3c7f8a87c4988f8adf43e57977b189f031f26f3e
```

The pinned package metadata labels PESTO LGPLv3 and explicitly includes
`weights/*.ckpt`. No separate checkpoint-specific terms were found in that
revision. MIRa Neural does not bundle or redistribute checkpoint bytes, exported
models, training recordings or datasets. The user supplies the pinned upstream
checkout and explicitly exports local assets. The MIR-1K training-data provenance
is not substituted for model-specific licensing evidence.

## Offline reference dependencies

PyTorch, Torchaudio, NumPy, SciPy and the packages listed in
`tools/requirements-reference.txt` are used only by the separately installed
export/reference environment. They are not embedded in or linked to the native
extension. Their own distributions provide their applicable license notices.

The optional server plugin builds against SuperCollider's GPL-licensed plugin
API. This software is provided without warranty, including implied warranties
of merchantability or fitness for a particular purpose. See the parent `LICENSE`
for the MIRa license.
