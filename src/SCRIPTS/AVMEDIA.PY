"""Generate owned audiovisual markers for independent output timing tests.

Each half-second scene is an eight-bar binary label: an anchor, a fixture bit
and a six-bit scene number. A 100 ms tone starts with every scene. The host
observer reads actual GDI pixels and the QEMU sound capture, not VLC clocks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lead', action='store_true', help='Add one second of unmarked black/silence before the 40 timed scenes')
args = parser.parse_args()
out = root/('BUILD/AV2MEDIA' if args.lead else 'BUILD/AVMEDIA')
out.mkdir(exist_ok=True)
records = []
for fixture, name, codec, frequency in [(0, 'AVH264.MP4', 'libx264', 441),
                                       (1, 'AVHEVC.MKV', 'libx265', 735)]:
    if args.lead:
        name = name.replace('AV', 'AV2', 1)
    video = out/'VIDEO.RGB'
    with video.open('wb') as stream:
        if args.lead:
            stream.write(bytes(64*48*3*10))
        for scene in range(40):
            bits = [1, fixture]+[(scene >> shift) & 1 for shift in range(5, -1, -1)]
            row = b''.join(bytes([255 if bit else 0])*24 for bit in bits)
            frame = row*48
            stream.write(frame*5)
    # Both frequencies have integer periods at 22050 Hz. Integer triangle
    # samples avoid a host math-library dependency in fixture construction.
    audio = out/'AUDIO.RAW'
    rate = 22050
    period = rate//frequency
    pcm = bytearray(rate*2 if args.lead else 0)
    for scene in range(40):
        for i in range(rate//2):
            phase = i % period
            sample = ((4*phase*6000//period-6000) if phase < period//2 else
                      (18000-4*phase*6000//period)) if i < rate//10 else 0
            pcm.extend(struct.pack('<h', sample))
    audio.write_bytes(pcm)
    command = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
               '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', '64x48',
               '-framerate', '10', '-i', str(video), '-f', 's16le', '-ar', str(rate),
               '-ac', '1', '-i', str(audio), '-c:v', codec, '-pix_fmt', 'yuv420p',
               '-preset', 'ultrafast', '-g', '10', '-bf', '2']
    if fixture:
        command += ['-x265-params', 'pools=none:frame-threads=1:log-level=error',
                    '-c:a', 'libopus', '-ar', '48000', '-b:a', '64k']
    else:
        command += ['-crf', '18', '-c:a', 'aac', '-b:a', '64k', '-movflags', '+faststart']
    command += [str(out/name)]
    subprocess.run(command, check=True)
    records.append({'fixture': fixture, 'file': name, 'scenes': 40,
                    'lead_seconds': int(args.lead), 'scene_seconds': .5, 'pulse_seconds': .1, 'frequency_hz': frequency,
                    'sha256': hashlib.sha256((out/name).read_bytes()).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records, indent=2)+'\n')
print('Generated two audiovisual fixtures with', int(args.lead), 'second(s) of unmarked lead')
