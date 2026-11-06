"""Owned audio fixtures with independently decoded host references."""
import hashlib
import json
import math
import struct
import subprocess
import wave
from pathlib import Path

root=Path(__file__).resolve().parents[1]
out=root/'BUILD'/'AFMEDIA';out.mkdir(exist_ok=True)
seed=7;pcm=[]
for i in range(96000):
    seed=(1664525*seed+1013904223)&0xffffffff
    t=i/48000
    envelope=min(1,t*20,(2-t)*20)
    sample=envelope*(5000*math.sin(2*math.pi*(211*t+137*t*t))+
                     1700*math.sin(2*math.pi*1789*t)+(seed/2**32-.5)*800)
    pcm.append(round(sample))
source=out/'SOURCE.WAV'
with wave.open(str(source),'wb') as f:
    f.setparams((1,2,48000,0,'NONE','not compressed'))
    f.writeframes(struct.pack('<'+'h'*len(pcm),*pcm))
formats=[('MP3','MP3','libmp3lame',[]),('FLAC','FLA','flac', ['-f','flac']),
 ('ALAC','M4A','alac',[]),('VORBIS','OGG','libvorbis',[]),('OPUS','OPU','libopus',['-f','ogg']),
 ('OPUSMKV','MKA','libopus',[]),('AC3','AC3','ac3',[]),('EAC3','EAC','eac3',['-f','eac3']),
 ('WMA1','WMA','wmav1',[]),('WMA2','WMA','wmav2',[]),
 ('WAVPACK','WV','wavpack',[]),('TTA','TTA','tta',[]),
 ('AIFF','AIF','pcm_s16be',['-f','aiff']),('MULAW','WAV','pcm_mulaw',[])]
records=[]
for stem,ext,codec,extra in formats:
    name=stem+'.'+ext
    subprocess.run(['ffmpeg','-v','error','-i',str(source),'-c:a',codec,
                    '-threads','1',*extra,'-y',str(out/name)],check=True)
    raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(out/name),
                                 '-ac','1','-ar','48000','-f','s16le','-'])
    (out/(stem+'.RAW')).write_bytes(raw)
    records.append({'file':name,'reference':stem+'.RAW','samples':len(raw)//2,
                    'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest(),
                    'reference_sha256':hashlib.sha256(raw).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records,indent=2)+'\n')
(out/'HOSTVER.TXT').write_bytes(subprocess.check_output(['ffmpeg','-version']))
(root/'BUILD'/'AFDATA.H').write_text('static const struct {const char *file,*raw;} cases[]={\n'+
    ''.join('{"'+r['file']+'","'+r['reference']+'"},\n' for r in records)+'};\n')
print('Generated '+str(len(records))+' audio formats and PCM references')
