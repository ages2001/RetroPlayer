"""Verify five short PCM clips in a closed emulated sound-device capture.

Checks ordered pulse lengths including clips below the startup threshold.
Sub-25 ms pulses do not provide enough frequency resolution for a pitch claim.
This does not certify physical hardware, every format or arbitrary seeking.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import wave
import numpy as np

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('bounds',type=Path)
p.add_argument('capture',type=Path)
p.add_argument('--output',required=True,type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
if a.output.resolve().parent!=root/'RESULTS' or not re.fullmatch('[A-Z0-9_]{1,8}\\.JSN',a.output.name):
    p.error('Output must be a DOS 8.3 report inside RESULTS')
bounds=json.loads(a.bounds.read_text())
with wave.open(str(a.capture),'rb') as w:
    rate,channels,width=w.getframerate(),w.getnchannels(),w.getsampwidth()
    if (rate,channels,width)!=(44100,2,2):raise SystemExit('Unexpected capture format')
    data=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,channels)
lo,hi=bounds['before'],bounds['after']
if not 0<=lo<hi<=len(data):raise SystemExit('Invalid observation bounds or unclosed capture')
mono=data[lo:hi].astype(float).mean(axis=1)
step=44
rms=np.sqrt(np.mean(mono[:len(mono)//step*step].reshape(-1,step)**2,axis=1))
groups=[]
for i in np.flatnonzero(rms>500):
    if not groups or i-groups[-1][-1]>2:groups.append([int(i)])
    else:groups[-1].append(int(i))
failures=[];pulses=[]
if bounds['guest_exit']!='EXIT 0':failures.append('Guest test failed')
expected=bounds['durations_ms']
if len(groups)!=len(expected):failures.append('Expected five pulses, observed '+str(len(groups)))
for i,g in enumerate(groups):
    start,end=g[0]*step,(g[-1]+1)*step
    duration=(end-start)*1000/rate
    frequency=None
    if duration>=25:
        signal=mono[start:end];power=np.abs(np.fft.rfft(signal*np.hanning(len(signal)),n=65536))
        frequency=float(np.argmax(power[1:])+1)*rate/65536
        if abs(frequency-441)>15:failures.append('Pulse %d frequency differs'%i)
    good=i<len(expected) and max(1,expected[i]-10)<=duration<=expected[i]+10
    if not good:failures.append('Pulse %d duration differs'%i)
    pulses.append({'start_seconds':(lo+start)/rate,'duration_ms':duration,'frequency_hz':frequency,'expected_ms':expected[i] if i<len(expected) else None})
r={'scope':__doc__,'capture':a.capture.name,'capture_sha256':hashlib.sha256(a.capture.read_bytes()).hexdigest(),
   'bounds':a.bounds.name,'bounds_sha256':hashlib.sha256(a.bounds.read_bytes()).hexdigest(),
   'pulses':pulses,'failures':failures,'result':'FAIL' if failures else 'PASS'}
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(bool(failures))
