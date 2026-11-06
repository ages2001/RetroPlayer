"""Add short, long-packet and chained Xiph fixtures to the audio matrix."""
import hashlib
import json
import math
import struct
import subprocess
import wave
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'BUILD' / 'AFMEDIA'
records = json.loads((out / 'REFS.JSN').read_text())

def encode(name, samples, codec, duration=None, phase=0):
    source = out / 'XSOURCE.WAV'
    pcm = [round(4500 * math.sin(2*math.pi*(317*i/48000 + 193*(i/48000)**2)+phase)
                 + 1300 * math.sin(2*math.pi*1703*i/48000)) for i in range(samples)]
    with wave.open(str(source), 'wb') as f:
        f.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        f.writeframes(struct.pack('<'+'h'*samples, *pcm))
    args = ['ffmpeg', '-v', 'error', '-i', str(source), '-c:a', codec, '-threads', '1']
    if duration is not None:
        args += ['-frame_duration', str(duration)]
    subprocess.run(args + ['-f', 'ogg', '-y', str(out/name)], check=True)

names = []
for ms in (1, 10, 40):
    for prefix, ext, codec in [('OP', 'OPU', 'libopus'), ('VO', 'OGG', 'libvorbis')]:
        name = f'{prefix}{ms:02d}.{ext}'
        encode(name, ms*48, codec)
        names.append(name)
for stem, duration in [('OP2MS', 2.5), ('OP60MS', 60)]:
    name = stem+'.OPU'
    encode(name, 96000, 'libopus', duration)
    names.append(name)
for prefix, ext, codec in [('OP', 'OPU', 'libopus'), ('VO', 'OGG', 'libvorbis')]:
    encode('PART1.OGG', 12000, codec)
    encode('PART2.OGG', 12000, codec, phase=.7)
    name = prefix+'CHAIN.'+ext
    (out/name).write_bytes((out/'PART1.OGG').read_bytes()+(out/'PART2.OGG').read_bytes())
    names.append(name)
for name in names:
    rawname = Path(name).stem+'.RAW'
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(out/name),
                                   '-ac', '1', '-ar', '48000', '-f', 's16le', '-'])
    (out/rawname).write_bytes(raw)
    records = [r for r in records if r['file'] != name]
    records.append({'file': name, 'reference': rawname, 'samples': len(raw)//2,
                    'sha256': hashlib.sha256((out/name).read_bytes()).hexdigest(),
                    'reference_sha256': hashlib.sha256(raw).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records, indent=2)+'\n')
print('Added '+str(len(names))+' Xiph boundary fixtures')
