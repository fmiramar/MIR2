# Neural pitch research gate

Audit date: 2026-10-24. This records the source and deployment feasibility audit
before implementation. On 2026-10-25, the selected PESTO prototype progressed to
the separate optional MIRaNeural extension; see the [implementation and measured
verification](neural-pitch-implementation.md). The base MIRa plugin remains
independent, and no model weights are committed or bundled.

## Initial audit decision (2026-10-24)

**Keep the base library native and dependency-free. Select PESTO v2 as the first
candidate for a separate optional prototype; do not merge a neural UGen yet.**
Its small encoder and existing streaming frontend make it a stronger starting
point than the other audited models. The acceptance blockers are specific:
checkpoint license scope must be established separately from source-code
licensing, frontend/decoder parity must be measured, and a native or external
worker implementation must demonstrate bounded per-hop time and memory on the
supported platforms. The source review does not establish those properties.

SPICE v2 and the FCPE paper checkpoint are excluded from the unrestricted base
bundle because their model terms are noncommercial. RMVPE is not selected for a
low-latency native port: its bidirectional sequence model, large network and
unresolved checkpoint provenance add substantial work. CREPE tiny is the useful
fallback for a fixed-window prototype, with a higher arithmetic cost than its
small parameter count suggests. These are engineering selection decisions,
not a claim that one model always estimates pitch more accurately.

## Exact sources inspected

| Project | Source revision | Artifact examined separately |
|---|---|---|
| CREPE | [`c9b71ce61491454125a0693f584f7244f29d9884`](https://github.com/marl/crepe/tree/c9b71ce61491454125a0693f584f7244f29d9884) | Author `models` branch [`bb29b8d99a89924476d112d27ed470e4ac5617c0`](https://github.com/marl/crepe/tree/bb29b8d99a89924476d112d27ed470e4ac5617c0) |
| PESTO | [`62bc0c9702558f19af4593752947fb9db1eadac9`](https://github.com/SonyCSLParis/pesto/tree/62bc0c9702558f19af4593752947fb9db1eadac9) | `pesto/weights/mir-1k_g7.ckpt` in that revision; Git blob `d68da3e2dbae4a39756509f8aa919fb320bd59b5`, 534,664 bytes |
| RMVPE | [`a6db1cd7d26014aa739383367afd9bab57fc624c`](https://github.com/Dream-High/RMVPE/tree/a6db1cd7d26014aa739383367afd9bab57fc624c) | No checkpoint in that source tree. Separate retraining fork [`0aabafba18289ca938a73af0b0297686abf4922d`](https://github.com/yxlllc/RMVPE/tree/0aabafba18289ca938a73af0b0297686abf4922d) reviewed for provenance only |
| FCPE | [`6a149c1afb1c7e7821b71869dfb31ad50c95b516`](https://github.com/CNChTu/FCPE/tree/6a149c1afb1c7e7821b71869dfb31ad50c95b516) | Author [DDSP_200k.pt model repository at `aeec888cd3eecd9a350945205a86340ef9293548`](https://huggingface.co/ChiTu/FCPE/tree/aeec888cd3eecd9a350945205a86340ef9293548); distinguish older bundled `fcpe_c_v001.pt` |
| SPICE | Model `google/spice/2`; [archived model-card revision `81fdea8ffa53b5acbd66a012d17fb8f9602c714b`](https://github.com/tensorflow/tfhub.dev/blob/81fdea8ffa53b5acbd66a012d17fb8f9602c714b/assets/docs/google/models/spice/2.md) | Versioned SavedModel; weights were not downloaded or hashed in this audit |

Git blob IDs identify inspected upstream objects; they are not SHA-256 download
checksums. A distributable artifact would additionally need its own SHA-256.

## Code and weight licenses

| Project | Source code | Model weights | Bundle decision |
|---|---|---|---|
| CREPE | [MIT](https://github.com/marl/crepe/blob/c9b71ce61491454125a0693f584f7244f29d9884/LICENSE) | The [MIT package installer](https://github.com/marl/crepe/blob/c9b71ce61491454125a0693f584f7244f29d9884/setup.py) downloads weights from the author's separate models branch. That branch contains five weight archives without a separate license file. Explicit checkpoint-specific terms were not found. | Do not silently assert a separate MIT weight grant; establish scope before redistribution. |
| PESTO | [LGPL-3.0](https://github.com/SonyCSLParis/pesto/blob/62bc0c9702558f19af4593752947fb9db1eadac9/LICENSE.md) | Checkpoints are in the licensed source repository, but the inspected README and weight directory do not separately state model-weight terms. Repository inclusion is evidence of provenance, not an independently established weight license. | Best prototype candidate; redistribution remains conditional. |
| RMVPE | Original author [Apache-2.0](https://github.com/Dream-High/RMVPE/blob/a6db1cd7d26014aa739383367afd9bab57fc624c/LICENSE) | The original tree supplies no weights. A retrained fork and downstream model collections are distinct artifacts; their labels cannot establish the original authors' checkpoint permissions. | No weights selected. |
| FCPE | [MIT](https://github.com/CNChTu/FCPE/blob/6a149c1afb1c7e7821b71869dfb31ad50c95b516/LICENSE) | The author [DDSP-200K model card](https://huggingface.co/ChiTu/FCPE/blob/aeec888cd3eecd9a350945205a86340ef9293548/README.md) explicitly declares CC BY-NC-SA 4.0. The older 43,363,260-byte bundled checkpoint is a different model; its weight-specific license was not established. | Exclude the paper checkpoint from the unrestricted base bundle. |
| SPICE | [TensorFlow runtime Apache-2.0](https://github.com/tensorflow/tensorflow/blob/master/LICENSE); this is not a model license | Official v2 model card explicitly states **CC BY-NC 4.0**. | Exclude from the unrestricted base bundle. |

No upstream maintainer was contacted. A source-code license, paper license,
training-dataset license and trained-weight license are separate audit items;
none is substituted for another here.

## Method, training data, size and timing

**CREPE.** The provided models use 16 kHz mono waveform frames of 1,024 samples
(64 ms), six convolutions and a 360-bin pitch classifier. Centered inference
needs 32 ms future context; a trailing implementation needs a full initial
frame, plus resampling/filter delay. Viterbi decoding adds sequence context and
is not appropriate for an unchanged low-latency callback. These follow the
[actual inference implementation](https://github.com/marl/crepe/blob/c9b71ce61491454125a0693f584f7244f29d9884/crepe/core.py).
The [authors' training list](https://github.com/marl/crepe/blob/c9b71ce61491454125a0693f584f7244f29d9884/README.md)
is MIR-1K, Bach10, RWC-Synth, MedleyDB, MDB-STEM-Synth and NSynth. Tiny/full
contain approximately 0.487M/22.244M stored scalar parameters. An independent
shape count from the code gives 36.79M/1,410.02M convolution/dense MACs per frame,
excluding normalization, activations and resampling. At a 10 ms hop, tiny alone
requires about 3.68 billion MACs/second before overhead. The author archives are
1,307,310 bytes (tiny) and 57,689,301 bytes (full), compressed; archive size is
not runtime memory. No local CPU real-time factor was measured.

**PESTO v2.** The selected research candidate is `mir-1k_g7`, trained on MIR-1K.
The [v2 paper](https://arxiv.org/html/2508.01488v1) describes a roughly 130k
parameter encoder over VQT frames and reports CPU RTF 0.0354 on an Intel Core
i9-12900H (2.9 GHz): 100 runs of five-second, 16 kHz segments with 10 ms hops,
including the inference pipeline. That is an author-reported batch timing,
not an SC callback benchmark. The
[frontend](https://github.com/SonyCSLParis/pesto/blob/62bc0c9702558f19af4593752947fb9db1eadac9/pesto/utils/hcqt.py)
uses cached convolutions for streaming; its context depends on VQT gamma,
lowest frequency, sample rate, hop and mirror/refill setting. Arbitrary input
sample rates are supported by rebuilding kernels. The paper's sub-10-ms claim
uses a particular streaming/refill configuration; it must not be assigned to
every checkpoint and frontend configuration. MIRa's existing VQT is not an
established numerically equivalent frontend.

**RMVPE.** The [original paper](https://arxiv.org/html/2306.15412v2) specifies
16 kHz audio, 256 log-mel bins, a 2,048-sample Hann window and 320-sample hop
(128/20 ms), with a U-Net and bidirectional GRU. It trains/evaluates on
MDB-stem-synth, MIR-1K, Cmedia and MIR ST500, with separate vocal/polyphonic
models. Training segments are 2.56 seconds. The backward recurrent path makes
unchanged full-sequence inference noncausal: segment length and padding affect
usable latency. A 20 ms hop is not 20 ms algorithmic latency. No original
checkpoint size or CPU RTF was verified. The separate
[retraining fork](https://github.com/yxlllc/RMVPE/blob/0aabafba18289ca938a73af0b0297686abf4922d/README.md)
uses processed MIR-1K/PTDB plus vocoder-synthesized M4Singer with 10 ms labels;
that provenance and frontend must not be conflated with the original paper.

**FCPE.** The [paper](https://arxiv.org/html/2509.15140v1) reports a 10.64M
parameter context network over 128-bin mel input, trained using DDSP-resynthesized
M4Singer and VCTK with augmentation. Its RTF 0.0062 is on an RTX 4090 GPU,
not a CPU result. The pinned tree's training configuration instead demonstrates
a 16 kHz, 1,024-window/160-hop, 512-bin STFT variant; it is not sufficient to
infer the paper checkpoint's frontend. Inference reads checkpoint configuration
and has noncausal context convolutions; resampling, feature padding and network
lookahead must all be measured for the chosen artifact. The paper model
repository occupies about 43.36 MB. No CPU RTF or bounded streaming latency was
verified. [Configuration](https://github.com/CNChTu/FCPE/blob/6a149c1afb1c7e7821b71869dfb31ad50c95b516/train/configs/config.yaml),
[inference wrapper](https://github.com/CNChTu/FCPE/blob/6a149c1afb1c7e7821b71869dfb31ad50c95b516/torchfcpe/models_infer.py).

**SPICE v2.** The official model card specifies mono 16 kHz input, internal CQT,
approximately 2.38M parameters and outputs centered every 512 samples (32 ms).
It was trained on MIR-1K mixed vocals/backing tracks. Boundary padding affects
first and last predictions. Its 32 ms output interval is not a documented
32 ms streaming latency. No CPU RTF or exact causal context was verified.
The model artifact is larger than v1; v1's 180k parameter count must not be
attributed to v2.

## Runtime and acceptance criteria

| Candidate | GPU requirement and existing runtime | Cross-platform/real-time consequence |
|---|---|---|
| CREPE | TensorFlow/Keras; CPU inference possible, GPU improves throughput | Needs exported fixed graph or native kernels plus matching 16 kHz resampler. No Python inside scsynth. |
| PESTO | PyTorch; CPU is supported; authors provide TorchScript/ONNX export | Most credible small optional native port. Freeze frontend configuration and explicit cache sizes; parity-check exported versus original outputs. |
| RMVPE | PyTorch with convolution and bidirectional recurrence; CPU execution possible | A bounded causal redesign changes model behavior. Prefer offline/worker experiment; no unsupported callback guarantee. |
| FCPE | PyTorch; no inherent GPU requirement, but cited speed is GPU-only | Fixed model/configuration and context are prerequisites; code export availability alone does not validate Windows/macOS/Linux latency. |
| SPICE | TensorFlow SavedModel; described as mobile-compatible | Runtime/model license distinction remains; CQT and padding require a separate streaming specification. |

A separate prototype must first pin model bytes, explicit weight terms,
preprocessing constants, decoder, precision and all dependency licenses. Then
measure initialization memory/time, warm steady-state per-hop median/p95/p99/max,
peak memory, deadline misses and end-to-end lag at 44.1/48/96 kHz. Compare
original-framework outputs and known pitch transitions, silence, noise and
out-of-range inputs. Report batch RTF separately from single-stream deadlines.
Finally verify native macOS x64/arm64, Windows x64 and Linux builds, scsynth and
supernova behavior, and publish a meaningful audible example. Until these pass,
the neural integration gate remains closed; the source audit and candidate
selection are complete.
