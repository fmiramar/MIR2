#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independently derive db1..db8 from Daubechies spectral factorization.

Only Python's standard library is used; no external coefficient table is read.
P(y) = sum(comb(N-1+k,k)*y**k, k=0..N-1).
For each root y choose the inside-unit-circle root of z*z-(2-4*y)*z+1.
Multiply these factors by (z+1)**N and normalize H(1)=sqrt(2).
Primary reference: Daubechies (1988), doi:10.1002/cpa.3160410705.

Usage:
  python3 tests/python/wavelet_filter_reference.py --check include/mir2/Wavelet.hpp
  python3 tests/python/wavelet_filter_reference.py --json
"""
import argparse
import cmath
import json
import math
from pathlib import Path
import re


def polynomial(coefficients, z):
    result = 0j
    for value in reversed(coefficients):
        result = result * z + value
    return result


def roots(coefficients):
    degree = len(coefficients) - 1
    if not degree:
        return []
    normalized = [v / coefficients[-1] for v in coefficients]
    values = [cmath.rect(1.2, 2 * math.pi * (j + .17) / degree) for j in range(degree)]
    for _ in range(1000):
        previous = values[:]
        corrections = []
        for index, value in enumerate(previous):
            denominator = 1 + 0j
            for other, root in enumerate(previous):
                if other != index:
                    denominator *= value - root
            corrections.append(polynomial(normalized, value) / denominator)
        values = [v - d for v, d in zip(previous, corrections)]
        if max(abs(d) for d in corrections) < 2e-15:
            break
    else:
        raise RuntimeError("Polynomial factorization did not converge")
    assert max(abs(polynomial(normalized, z)) for z in values) < 1e-12
    return sorted(values, key=lambda z: (z.real, z.imag))


def convolve(a, b):
    result = [0j] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            result[i+j] += x*y
    return result


def daubechies(order):
    factors = roots([math.comb(order-1+k, k) for k in range(order)])
    coefficients = [complex(math.comb(order, k)) for k in range(order+1)]
    for y in factors:
        middle = 2 - 4*y
        radical = cmath.sqrt(middle*middle - 4)
        z = min([(middle+radical)/2, (middle-radical)/2], key=abs)
        assert abs(z) < 1
        coefficients = convolve(coefficients, [1, -z])
    scale = math.sqrt(2) / sum(coefficients)
    coefficients = [c * scale for c in coefficients]
    assert max(abs(c.imag) for c in coefficients) < 1e-13
    values = [c.real for c in coefficients]
    for offset in range(0, len(values), 2):
        dot = sum(values[i] * values[i+offset] for i in range(len(values)-offset))
        assert abs(dot - (1 if offset == 0 else 0)) < 3e-14
    # Normalize coordinates to avoid large polynomial powers masking cancellation.
    for moment in range(order):
        residual = sum(((-1)**k)*values[-1-k]*(k/(2*order))**moment for k in range(2*order))
        assert abs(residual) < 3e-14
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', type=Path)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    generated = {order: daubechies(order) for order in range(1, 9)}
    if args.check:
        source = args.check.read_text()
        blocks = re.findall(r'(?:case\s+(\d+):|default:)\s*return\s*\{\{(.*?)\},\s*(\d+)\};', source, re.S)
        table = {}
        for label, body, taps in blocks:
            order = int(label) if label else int(taps)//2
            table[order] = [float(v.strip()) for v in body.split(',') if v.strip()]
        assert set(table) == set(generated), f'Unexpected implemented orders: {sorted(table)}'
        worst = 0
        for order, values in generated.items():
            assert len(table[order]) == 2*order
            worst = max(worst, max(abs(x-y) for x,y in zip(values, table[order])))
        assert worst < 3e-14, worst
        print(f'db1..db8 spectral-factor coefficient oracle passed; largest difference {worst:.3g}')
    if args.json:
        print(json.dumps(generated, indent=2))


if __name__ == '__main__':
    main()
