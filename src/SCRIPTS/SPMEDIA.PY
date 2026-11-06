"""Owned Speex fixtures and independently decoded native-rate references."""
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import wave

root=Path(__file__).resolve().parents[1]
out=root/'BUILD/SPMEDIA';out.mkdir(exist_ok=True)
records=[]
def fixture(stem,rate,channels,ms,variant=0,packet=1):
    source=out/(stem+'.WAV');pcm=[];seed=11+variant
    for i in range(rate*ms//1000):
        t=i/rate
        for channel in range(channels):
            seed=(1664525*seed+1013904223)&0xffffffff
            # Distinct channels and chirp slopes expose swaps and delay errors.
            value=5000*math.sin(2*math.pi*((173+157*channel+79*variant)*t+(97+37*channel)*t*t))
            value+=1200*math.sin(2*math.pi*min(1789,rate//5)*t)+(seed/2**32-.5)*500
            pcm.append(round(value))
    with wave.open(str(source),'wb') as w:
        w.setparams((channels,2,rate,0,'NONE','not compressed'));w.writeframes(struct.pack('<'+'h'*len(pcm),*pcm))
    file=out/(stem+'.SPX')
    subprocess.run(['ffmpeg','-v','error','-i',str(source),'-c:a','libspeex','-cbr_quality','8',
        '-frames_per_packet',str(packet),'-threads','1','-f','ogg','-y',str(file)],check=True)
    # Force the independent host libspeex adapter, not FFmpeg's alternative implementation.
    raw=subprocess.check_output(['ffmpeg','-v','error','-c:a','libspeex','-i',str(file),'-f','s16le','-'])
    reference=out/(stem+'.RAW');reference.write_bytes(raw)
    record={'file':file.name,'reference':reference.name,'rate':rate,'channels':channels,'source_ms':ms,
        'frames_per_packet':packet,'samples':len(raw)//2,'reference_decoder':'Host FFmpeg libspeex, native rate and channels',
        'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'reference_sha256':hashlib.sha256(raw).hexdigest()}
    records.append(record);return record
for stem,rate,channels,ms,packet in [('SPNB',8000,1,1000,1),('SPWB',16000,1,1000,1),
    ('SPUWB',32000,1,1000,1),('SPST',16000,2,1000,1),('SP10',16000,1,10,1),
    ('SP20',8000,1,20,1),('SP70',16000,1,70,1),('SPPACK',16000,1,1000,4),
    ('SPLONG',16000,1,2300,1)]:
    fixture(stem,rate,channels,ms,packet=packet)
links=[fixture('SPLINK'+str(i),16000,1,400,variant=i) for i in range(2)]
records=records[:-2]
file=out/'SPCHAIN.SPX';file.write_bytes(b''.join((out/r['file']).read_bytes() for r in links))
raw=b''.join((out/r['reference']).read_bytes() for r in links);(out/'SPCHAIN.RAW').write_bytes(raw)
records.append({'file':file.name,'reference':'SPCHAIN.RAW','rate':16000,'channels':1,'source_ms':800,
    'samples':len(raw)//2,'reference_decoder':'Host FFmpeg libspeex; fresh decoder per logical stream',
    'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'reference_sha256':hashlib.sha256(raw).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records,indent=2)+'\n')
(out/'HOSTVER.TXT').write_bytes(subprocess.check_output(['ffmpeg','-version']))
print('Generated',len(records),'Speex cases')
