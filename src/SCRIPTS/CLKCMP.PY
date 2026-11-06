"""Compare diagnostic VLC, Windows and raw TSC clocks with the waveOut cursor.

The TSC scale is estimated from the median of Windows-counter intervals.
This identifies relative discontinuities; it is not an absolute-frequency
calibration or a physical-device acceptance test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('log', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if a.output.exists():
    p.error('Preserve prior diagnostic reports; choose a new output')
result = {'scope': __doc__, 'log': a.log.name,
          'log_sha256': hashlib.sha256(a.log.read_bytes()).hexdigest(),
          'fixtures': [], 'result': 'DIAGNOSTIC'}
for number, text in enumerate(a.log.read_text().split('RSTART begin=')[1:]):
    rate = int(re.search(r'rate=(\d+)', text)[1])
    rows = []
    for line in text.splitlines():
        if not line.startswith('RCLOCK '):
            continue
        row = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}
        if 'tsc_hi' not in row or 'tsc_lo' not in row:
            p.error('Raw TSC observations are required')
        row['tsc'] = (row['tsc_hi'] << 32) + row['tsc_lo']
        rows.append(row)
    intervals = [(b['tsc'] - c['tsc']) / (b['perf'] - c['perf'])
                 for c, b in zip(rows, rows[1:])
                 if b['perf'] - c['perf'] >= 10000 and b['tsc'] > c['tsc']]
    if len(intervals) < 5:
        p.error('Insufficient increasing processor-counter observations')
    scale = statistics.median(intervals)
    for row in rows:
        counter_us = row['tsc'] / scale
        row['windows_minus_tsc_ms'] = (row['perf'] - counter_us) / 1000
        row['vlc_minus_tsc_ms'] = (row['now'] - counter_us) / 1000
        row['native_origin_error_ms'] = (
            counter_us - row['sample'] * 1000000 / rate - row['origin']) / 1000
    result['fixtures'].append({
        'fixture': number, 'rate': rate, 'tsc_ticks_per_us_estimate': scale,
        'minimum_windows_minus_tsc_ms': min(r['windows_minus_tsc_ms'] for r in rows),
        'maximum_vlc_tsc_difference_ms': max(abs(r['vlc_minus_tsc_ms']) for r in rows),
        'warnings': [line for line in text.splitlines()
                     if 'sampling' in line or 'drift' in line], 'observations': rows})
if not result['fixtures']:
    p.error('No diagnostic fixtures found')
a.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
for item in result['fixtures']:
    print('Fixture', item['fixture'], 'Windows/TSC minimum ms',
          round(item['minimum_windows_minus_tsc_ms'], 3), 'VLC/TSC max ms',
          round(item['maximum_vlc_tsc_difference_ms'], 3))
