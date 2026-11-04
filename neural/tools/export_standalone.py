#!/usr/bin/env python3
"""Standalone exporter for PESTO mir-1k_g7 model.
Requires only NumPy and standard Python libraries (no PyTorch required).
Reads weights from the pinned mir-1k_g7.ckpt and computes CQT frontend kernels.
"""

import io
import pickle
import struct
import zipfile
import numpy as np
from pathlib import Path

WEIGHT_NAMES = [
    "encoder.layernorm.weight", "encoder.layernorm.bias",
    "encoder.conv1.0.weight", "encoder.conv1.0.bias",
    "encoder.prefilt_layers.0.0.weight", "encoder.prefilt_layers.0.0.bias",
    "encoder.prefilt_layers.1.0.weight", "encoder.prefilt_layers.1.0.bias",
    "encoder.conv_layers.0.weight", "encoder.conv_layers.0.bias",
    "encoder.conv_layers.3.weight", "encoder.conv_layers.3.bias",
    "encoder.conv_layers.6.weight", "encoder.conv_layers.6.bias",
    "encoder.conv_layers.9.weight", "encoder.conv_layers.9.bias",
    "encoder.fc.weight", "confidence.conv.weight", "confidence.conv.bias",
    "confidence.linear.weight", "confidence.linear.bias", "shift",
]

def load_weights(ckpt_path):
    class FakeClass:
        def __init__(self, *args, **kwargs):
            self.args = args

    class FakeUnpickler(pickle.Unpickler):
        def find_class(self, module, name):
            if module == 'collections' and name == 'OrderedDict':
                import collections
                return collections.OrderedDict
            return type(name, (FakeClass,), {'__module__': module})
        def persistent_load(self, pid):
            return pid

    with zipfile.ZipFile(ckpt_path, 'r') as z:
        b = z.read('mir-1k_g7/data.pkl')
        obj = FakeUnpickler(io.BytesIO(b)).load()
        state_dict = obj['state_dict']
        
        weights = []
        for name in WEIGHT_NAMES:
            entry = state_dict[name]
            storage_info = entry.args[0]
            storage_id = storage_info[2]
            raw_bytes = z.read(f'mir-1k_g7/data/{storage_id}')
            arr = np.frombuffer(raw_bytes, dtype='<f4')
            weights.append(arr)
            
        weights = np.concatenate(weights).astype('<f4', copy=False)
        return weights

def compute_cqt_kernels(rate, n_bins=251, bins_per_semitone=3, gamma=7):
    bins_per_octave = 12 * bins_per_semitone
    fmin = 27.5 / (2.0 ** ((bins_per_semitone - 1) / (24.0 * bins_per_semitone)))
    Q = 1.0 / (2.0 ** (1.0 / bins_per_octave) - 1.0)
    alpha = 2.0 ** (1.0 / bins_per_octave) - 1.0
    
    freqs = fmin * (2.0 ** (np.arange(n_bins) / float(bins_per_octave)))
    lengths = np.ceil(Q * rate / (freqs + gamma / alpha))
    max_len = int(max(lengths))
    fftLen = int(2 ** int(np.ceil(np.log2(max_len))))
    
    tempKernel = np.zeros((n_bins, fftLen), dtype=np.complex64)
    for k in range(n_bins):
        freq = freqs[k]
        l = int(lengths[k])
        if l % 2 == 1:
            start = int(np.ceil(fftLen / 2.0 - l / 2.0)) - 1
        else:
            start = int(np.ceil(fftLen / 2.0 - l / 2.0))
        
        # Periodic Hann window:
        n = np.arange(l, dtype=np.float64)
        w = 0.5 - 0.5 * np.cos(2.0 * np.pi * n / float(l))
        
        # Complex exponential with time centering:
        t_idx = np.arange(-l // 2, l // 2, dtype=np.float64)
        sig = w * np.exp(t_idx * 1j * 2.0 * np.pi * freq / float(rate)) / float(l)
        
        # L1 norm normalization (norm=1):
        norm_val = np.sum(np.abs(sig))
        if norm_val > 0:
            sig = sig / norm_val
        tempKernel[k, start:start + l] = sig
        
    sqrt_lengths = np.sqrt(lengths).astype('<f4')
    return tempKernel, fftLen, sqrt_lengths

def export_model(ckpt_path, out_dir, rate):
    weights = load_weights(ckpt_path)
    if weights.size != 130575:
        raise ValueError(f"Expected 130575 weights, got {weights.size}")
        
    n_bins = 251
    hop = int(0.01 * rate + 0.5)
    kernels_complex, width, sqrt_lengths = compute_cqt_kernels(rate, n_bins=n_bins)
    
    # kernels array is real part followed by -imag part
    real_part = kernels_complex.real.astype('<f4')
    imag_part = -kernels_complex.imag.astype('<f4')
    kernels = np.vstack([real_part, imag_part])
    
    compact, descriptors, offset = [], [], 0
    for index in range(n_bins):
        r, im = kernels[index], kernels[index + n_bins]
        nonzero = np.flatnonzero((r != 0) | (im != 0))
        if not nonzero.size:
            raise RuntimeError("Empty frontend kernel")
        start, end = int(nonzero[0]), int(nonzero[-1] + 1)
        length = end - start
        record = [float(start), float(length), float(offset), float(offset + length), float(sqrt_lengths[index])]
        descriptors.append(record)
        compact.extend([r[start:end], im[start:end]])
        offset += 2 * length
        
    compact = np.concatenate(compact).astype('<f4', copy=False)
    descriptors = np.asarray(descriptors, dtype='<f4').reshape(-1)
    total = 32 + weights.size + descriptors.size + compact.size
    
    header = np.zeros(32, dtype='<f4')
    header[:9] = [734201, 1, rate, hop, width, n_bins, weights.size, compact.size, total]
    packed = np.concatenate([header, weights, descriptors, compact]).astype('<f4', copy=False)
    
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    
    wav_path = out_dir / f"mir-1k_g7-{rate}.wav"
    data_bytes = packed.tobytes()
    wav_header = struct.pack(
        '<4sI4s4sIHHIIHH4sI',
        b'RIFF', 36 + len(data_bytes), b'WAVE',
        b'fmt ', 16, 3, 1, rate, rate * 4, 4, 32,
        b'data', len(data_bytes)
    )
    with open(wav_path, 'wb') as f:
        f.write(wav_header)
        f.write(data_bytes)
        
    print(f"Exported {wav_path} ({wav_path.stat().st_size} bytes, {packed.size} samples)")
    return wav_path

if __name__ == '__main__':
    ckpt = 'build/pesto-weights/mir-1k_g7.ckpt'
    out = 'neural/models'
    for sr in (16000, 44100, 48000, 96000):
        export_model(ckpt, out, sr)
