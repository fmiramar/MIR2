#!/usr/bin/env python3
"""Aggregate the deterministic onset sweep; never tunes individual recordings."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('input', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
groups = defaultdict(lambda: [0, 0, 0, 0.0])
with args.input.open() as handle:
    for row in csv.DictReader(handle):
        group = groups[(row['implementation'], row['method'], float(row['threshold']))]
        tp, fp, fn = (int(row[key]) for key in ('tp', 'fp', 'fn'))
        group[0] += tp
        group[1] += fp
        group[2] += fn
        group[3] += float(row['mean_error_ms']) * tp
rows = []
for (implementation, method, threshold), (tp, fp, fn, timing) in sorted(groups.items()):
    precision = tp / (tp + fp) if tp + fp else 0.0
    recall = tp / (tp + fn) if tp + fn else 0.0
    f1 = 2 * tp / (2 * tp + fp + fn) if 2 * tp + fp + fn else 0.0
    rows.append(dict(implementation=implementation, method=method, threshold=threshold,
                     tp=tp, fp=fp, fn=fn, precision=precision, recall=recall, f1=f1,
                     mean_error_ms=timing / tp if tp else 0.0))
with args.output.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)
for implementation, method in sorted({(r['implementation'], r['method']) for r in rows}):
    best = max((r for r in rows if r['implementation'] == implementation and r['method'] == method),
               key=lambda r: (r['f1'], -r['threshold']))
    print('| {implementation} | {method} | {threshold:g} | {tp} | {fp} | {fn} | {f1:.3f} | {mean_error_ms:.1f} |'.format(**best))
print('Default SuperFlux:')
for row in rows:
    if row['method'] == 'superflux' and row['threshold'] == 0.3:
        print(row)
