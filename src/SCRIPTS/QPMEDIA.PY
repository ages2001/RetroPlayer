"""Owned Opus tone sequences for bounded waveOut queue validation."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import wave

root=Path(__file__).resolve().parents[1]
out=root/'BUILD/QPMEDIA';out.mkdir(exist_ok=True)
rate=48000
pcm=[]
for i in range(rate*9):
    t=i/rate
    frequency=next((f for begin,f in ((.5,441),(3.5,735),(6.5,315)) if begin<=t<begin+2),None)
    if frequency is None:value=0
    else:
        phase=(i*frequency)%rate
        value=round((phase if phase<rate//2 else rate-phase)*24000/rate-6000)
    pcm.append(value)
source=out/'QPINPUT.WAV'
with wave.open(str(source),'wb') as w:
    w.setparams((1,2,rate,0,'NONE','not compressed'))
    w.writeframes(struct.pack('<'+'h'*len(pcm),*pcm))
records=[]
for name,duration in [('PACK2',2.5),('PACK20',20),('PACK60',60)]:
    path=out/(name+'.OPU')
    subprocess.run(['ffmpeg','-v','error','-i',str(source),'-c:a','libopus','-application','audio',
                    '-frame_duration',str(duration),'-b:a','96000','-threads','1','-f','ogg','-y',str(path)],check=True)
    packets=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries',
        'packet=duration_time','-of','json',str(path)]))['packets']
    reference=subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-f','s16le','-'])
    assert len(reference)==len(pcm)*2
    records.append({'file':path.name,'packet_ms':duration,'packet_count':len(packets),
        'duration_seconds':9,'samples':len(reference)//2,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'decoded_sha256':hashlib.sha256(reference).hexdigest(),
        'expected_hz':[441,735,315],'tone_seconds':2,'inter_tone_silence_seconds':1})
(out/'REFS.JSN').write_text(json.dumps(records,indent=2)+'\n')
print('Generated',len(records),'Opus device-output fixtures')
