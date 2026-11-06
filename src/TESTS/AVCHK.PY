"""Verify that the independent timing analyzer rejects known bad recordings.

These synthetic recordings are analyzer tests, never guest playback evidence.
"""
import json
from pathlib import Path
import subprocess
import sys
import wave

import numpy as np

root = Path(__file__).resolve().parents[1]
work = root/'BUILD/AVCHECK'
work.mkdir(exist_ok=True)
rate = 44100
audio = np.zeros((42*rate, 2), dtype='<i2')
for fixture in range(2):
    for scene in range(40):
        start = int((1+fixture*20+scene*.5)*rate)
        t = np.arange(rate//10)
        period = rate//(441, 735)[fixture]
        tone = (6000*np.sin(2*np.pi*t/period)).astype('<i2')
        audio[start:start+len(tone)] = tone[:, None]
capture = work/'SIGNAL.WAV'
with wave.open(str(capture), 'wb') as stream:
    stream.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
    stream.writeframes(audio.tobytes())
for name, shift, missing, expected in [('AVCPASS', 0, False, 'PASS'),
                                        ('AVCDRFT', .3, False, 'FAIL'),
                                        ('AVCMISS', 0, True, 'FAIL')]:
    observations = []
    for index in range(1051):
        seconds = index*.04
        media = seconds-1-shift
        marker = None
        if 0 <= media < 40:
            fixture = int(media)//20
            scene = int((media-fixture*20)*2)
            marker = [fixture, scene]
            if missing and fixture == 0 and scene == 17:
                marker = [0, 16]
        cursor = min(len(audio), round(seconds*rate))
        observations.append({'before': cursor, 'after': cursor,
                             'snapshot_seconds': .002, 'marker': marker})
    trace = work/(name+'.JSN')
    trace.write_text(json.dumps({'guest': 'SYNTHETIC ANALYZER TEST',
        'audio': {'rate': rate}, 'result': 'CAPTURED', 'guest_exit': 'EXIT 0',
        'samples': observations}))
    report = root/'RESULTS'/(name+'.JSN')
    result = subprocess.run([sys.executable, str(root/'SCRIPTS/AVCHK.PY'),
                             str(trace), str(capture), '--output', str(report)], capture_output=True, text=True)
    actual = json.loads(report.read_text())['result']
    if actual != expected or result.returncode != (expected == 'FAIL'):
        raise SystemExit(name+' unexpected analyzer result: '+result.stdout+result.stderr)
    print(name, 'expected', expected, 'observed', actual)
