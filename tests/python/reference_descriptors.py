#!/usr/bin/env python3
"""Optional independent descriptor calculations for MIR2 development."""

from __future__ import annotations

import argparse
import json
import math

def centroid(magnitude: list[float], sample_rate: float, fft_size: int) -> float:
    total = sum(magnitude)
    weighted = sum(
        index * sample_rate / fft_size * value
        for index, value in enumerate(magnitude)
    )
    return 0.0 if total == 0.0 else weighted / total


def flatness(power: list[float], floor: float = 1.0e-20) -> float:
    if sum(power) == 0.0:
        return 0.0
    safe = [max(value, floor) for value in power]
    geometric = math.exp(sum(math.log(value) for value in safe) / len(safe))
    return geometric / (sum(safe) / len(safe))


def percentile(power: list[float], sample_rate: float, fft_size: int,
               fraction: float) -> float:
    total = sum(power)
    if total == 0.0:
        return 0.0
    target = fraction * total
    accumulated = 0.0
    for index, value in enumerate(power):
        accumulated += value
        if accumulated >= target:
            return index * sample_rate / fft_size
    return (len(power) - 1) * sample_rate / fft_size


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample-rate", type=float, default=48000.0)
    parser.add_argument("--fft-size", type=int, default=1024)
    parser.add_argument("--bin", type=int, default=10)
    args = parser.parse_args()
    magnitude = [0.0] * (args.fft_size // 2 + 1)
    magnitude[args.bin] = 1.0
    power = [value * value for value in magnitude]
    result = {
        "centroid_hz": centroid(magnitude, args.sample_rate, args.fft_size),
        "flatness": flatness(power),
        "power_percentile_0_85_hz": percentile(
            power, args.sample_rate, args.fft_size, 0.85
        ),
    }
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
