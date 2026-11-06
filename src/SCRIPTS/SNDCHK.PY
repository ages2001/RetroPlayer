"""Verify AOUTTEST's ordered tones in a closed QEMU audio output capture.

This is emulated-device evidence, not certification of physical hardware.
The capture can contain boot sounds and silence before/after the test.
"""
import argparse
import json
import wave
from pathlib import Path
import numpy as np

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('capture',type=Path)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--bounds',type=Path,help='Restrict analysis to recorded host sample bounds')
p.add_argument('--sequences',type=int,choices=(1,3),default=1,help='One tone triad, or three Opus packet-size triads')
p.add_argument('--raw',action='store_true',help='Capture is RAWTEST, without LibVLC')
p.add_argument('--device',choices=('hda','sb16'),default='hda')
a=p.parse_args()
with wave.open(str(a.capture),'rb') as stream:
    rate=stream.getframerate();channels=stream.getnchannels()
    if stream.getsampwidth()!=2:raise SystemExit('Expected signed 16-bit PCM capture')
    data=np.frombuffer(stream.readframes(stream.getnframes()),dtype='<i2').reshape(-1,channels)
if not len(data):raise SystemExit('Capture is empty or QEMU has not closed its WAV header')
offset=0
if a.bounds:
    bounds=json.loads(a.bounds.read_text())
    offset,end=bounds['before'],bounds['after']
    if not 0<=offset<end<=len(data):raise SystemExit('Invalid sound capture bounds')
    data=data[offset:end]
mono=data.astype(np.float64).mean(axis=1)
freqs=[441,735,315]
hop=rate//10
t=np.arange(hop)/rate
templates=np.exp(-2j*np.pi*np.array(freqs)[:,None]*t)
labels=[]
for pos in range(0,len(mono)-hop+1,hop):
    block=mono[pos:pos+hop];energy=float(block@block)
    powers=2*np.abs(templates@block)**2/(hop*max(energy,1))
    label=int(np.argmax(powers))
    labels.append(label if energy/hop>=50**2 and powers[label]>.80 else -1)
runs=[]
i=0
while i<len(labels):
    if labels[i]<0:i+=1;continue
    start=i;label=labels[i]
    while i<len(labels) and labels[i]==label:i+=1
    if (i-start)*hop/rate>=1.5:runs.append((label,start*hop,i*hop))
sequences=[]
for i in range(len(runs)-3*a.sequences+1):
    candidate=runs[i:i+3*a.sequences]
    if [r[0] for r in candidate]==[0,1,2]*a.sequences:
        sequences.append(candidate)
sequence=sequences[0] if sequences else None
result={'scope':(('Direct guest waveOut driver and emulated ' if a.raw else
                 'LibVLC through guest waveOut driver and emulated ')+a.device.upper()+' output'),
        'capture':a.capture.name,'device':a.device,'sample_rate':rate,'channels':channels,
        'duration_seconds':len(mono)/rate,'sequences':a.sequences,
        'sample_offset':offset,'bounds':a.bounds.name if a.bounds else None,'tones':[],'failures':[]}
if a.sequences>1 and len(runs)!=3*a.sequences:
    result['failures'].append('Unexpected number of long tone segments')
if len(sequences)>1:
    result['failures'].append('Ambiguous tone sequences; use the recorded bounds for the intended test')
if sequence is None:result['failures'].append('Missing ordered 441/735/315 Hz sequence')
else:
    for index,(label,start,end) in enumerate(sequence):
        duration=(end-start)/rate
        # Exclude the windows nearest the transition to/from silence.
        lo=start+rate//5;hi=end-rate//5;core=mono[lo:hi]
        spectrum=np.abs(np.fft.rfft(core*np.hanning(len(core))))
        peak=int(np.argmax(spectrum[1:]))+1
        left,center,right=np.log(spectrum[peak-1:peak+2]+1e-12)
        correction=.5*(left-right)/(left-2*center+right)
        measured=(peak+correction)*rate/len(core)
        rms=float(np.sqrt(np.mean(core**2)))
        quiet=np.abs(core)<5
        edges=np.diff(np.r_[False,quiet,False].astype(np.int8))
        lengths=np.flatnonzero(edges==-1)-np.flatnonzero(edges==1)
        longest=float(max(lengths,default=0))/rate
        correlation=1.0
        if channels==2:
            left=data[lo:hi,0];right=data[lo:hi,1]
            correlation=float(np.corrcoef(left,right)[0,1]) if left.std()>0 and right.std()>0 else 0.0
        tone={'expected_hz':freqs[label],'measured_hz':round(float(measured),3),
              'start_seconds':(start+offset)/rate,'duration_seconds':duration,'rms_s16':round(rms,2),
              'longest_near_zero_seconds':longest,'stereo_correlation':correlation}
        result['tones'].append(tone)
        if abs(measured-freqs[label])>max(1,freqs[label]*.002):result['failures'].append(f'Tone {index}: wrong frequency')
        if not 1.7<=duration<=2.3:result['failures'].append(f'Tone {index}: wrong duration')
        if not 50<=rms<=16000:result['failures'].append(f'Tone {index}: invalid output level')
        if longest>.005:result['failures'].append(f'Tone {index}: audio dropout')
        if not correlation>=.99:result['failures'].append(f'Tone {index}: mono stereo channels differ')
        if np.any(np.abs(data[lo:hi].astype(np.int32))>=32760):result['failures'].append(f'Tone {index}: clipping')
        if index%3 and not .7<=(start-sequence[index-1][2])/rate<=1.3:
            result['failures'].append(f'Tone {index}: incorrect silence interval')
result['result']='FAIL' if result['failures'] else 'PASS'
a.output.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
print(json.dumps(result,indent=2,allow_nan=False))
raise SystemExit(bool(result['failures']))
