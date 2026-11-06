"""Independent analyzer self-checks; these are not guest playback evidence."""
import json
from pathlib import Path
import subprocess
import sys
import wave
import numpy as np

root=Path(__file__).resolve().parents[1]
work=root/'BUILD/QUCHECK';work.mkdir(exist_ok=True)
rate=44100;durations=[5,20,70,100,200]
for name,missing,short,extra in [('QCPASS',False,False,False),('QCLOSS',True,False,False),('QCSHORT',False,True,False),('QCEXTRA',False,False,True)]:
    data=np.zeros((7*rate,2),dtype='<i2')
    for i,ms in enumerate(durations):
        if missing and i==0:continue
        if short and i==2:ms=20
        n=rate*ms//1000;start=(i+1)*rate
        signal=(6000*np.sin(2*np.pi*441*np.arange(n)/rate)).astype('<i2')
        data[start:start+n]=signal[:,None]
    if extra:
        n=rate//50;signal=(6000*np.sin(2*np.pi*441*np.arange(n)/rate)).astype('<i2');data[6*rate:6*rate+n]=signal[:,None]
    capture=work/'SIGNAL.WAV'
    with wave.open(str(capture),'wb') as w:w.setparams((2,2,rate,0,'NONE','not compressed'));w.writeframes(data.tobytes())
    bounds=work/'BOUNDS.JSN';bounds.write_text(json.dumps({'before':0,'after':len(data),'guest_exit':'EXIT 0','durations_ms':durations}))
    output=root/'RESULTS'/(name+'.JSN')
    r=subprocess.run([sys.executable,str(root/'SCRIPTS/QUCHK.PY'),str(bounds),str(capture),'--output',str(output)],capture_output=True,text=True)
    expected='PASS' if name=='QCPASS' else 'FAIL';observed=json.loads(output.read_text())['result']
    if observed!=expected or r.returncode!=(expected=='FAIL'):raise SystemExit(name+' unexpected result: '+r.stdout+r.stderr)
    print(name,'expected',expected,'observed',observed)
