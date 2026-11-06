"""Positive and missing-triad checks for the independent packet-tone analyzer."""
import json
from pathlib import Path
import subprocess
import sys
import wave
import numpy as np
root=Path(__file__).resolve().parents[1]
rate=44100
samples=np.zeros(rate*30,dtype=np.int16)
for sequence in range(3):
    for index,f in enumerate((441,735,315)):
        start=rate*(sequence*10+index*3)
        t=np.arange(rate*2)/rate
        samples[start:start+len(t)]=(3000*np.sin(2*np.pi*f*t)).astype(np.int16)
checks={}
for name,missing in [('QPGOOD',False),('QPLOSS',True)]:
    pcm=samples.copy()
    if missing:pcm[rate*26:rate*28]=0
    path=root/'RESULTS'/(name+'.WAV')
    with wave.open(str(path),'wb') as w:
        w.setparams((2,2,rate,0,'NONE','not compressed'))
        w.writeframes(np.repeat(pcm[:,None],2,axis=1).astype('<i2').tobytes())
    report=root/'RESULTS'/(name+'.JSN')
    result=subprocess.run([sys.executable,str(root/'SCRIPTS/SNDCHK.PY'),str(path),
        '--sequences','3','--output',str(report)],capture_output=True,text=True)
    actual=json.loads(report.read_text())
    checks[name]=result.returncode==(1 if missing else 0) and actual['result']==('FAIL' if missing else 'PASS')
(root/'RESULTS/QPHOST.JSN').write_text(json.dumps(checks,indent=2)+'\n')
print(checks)
raise SystemExit(not all(checks.values()))
