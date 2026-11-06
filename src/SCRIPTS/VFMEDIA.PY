"""Generate owned video/container fixtures and independent decoded hashes."""
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root/'BUILD/VFMEDIA'
out.mkdir(exist_ok=True)
source = out/'SOURCE.YUV'
source.write_bytes(subprocess.check_output(['ffmpeg', '-v', 'error', '-f', 'lavfi', '-i',
    'testsrc2=size=64x48:rate=6:duration=2', '-pix_fmt', 'yuv420p', '-f', 'rawvideo', '-']))
formats = [
    ('MPEG1.MPG', 'mpeg1video', 'yuv420p', ['-bf', '2', '-f', 'mpeg']),
    ('MPEG2.MPG', 'mpeg2video', 'yuv420p', ['-bf', '2', '-f', 'mpeg']),
    ('MPEG4.AVI', 'mpeg4', 'yuv420p', ['-bf', '2']),
    ('MSMPEG4.AVI', 'msmpeg4', 'yuv420p', []),
    ('WMV1.WMV', 'wmv1', 'yuv420p', ['-f', 'asf']),
    ('WMV2.WMV', 'wmv2', 'yuv420p', ['-f', 'asf']),
    ('THEORA.OGV', 'libtheora', 'yuv420p', ['-f', 'ogg']),
    ('VP8.WEB', 'libvpx', 'yuv420p', ['-f', 'webm']),
    ('FFV1.MKV', 'ffv1', 'yuv420p', ['-level', '3']),
    ('FFVHUFF.AVI', 'ffvhuff', 'yuv420p', []),
    ('MJPEG.AVI', 'mjpeg', 'yuvj420p', []),
    ('JPEG2000.MKV', 'jpeg2000', 'yuv420p', []),
    ('FLV1.FLV', 'flv', 'yuv420p', ['-f', 'flv']),
    ('CINEPAK.AVI', 'cinepak', 'rgb24', []),
    ('TSH264.TS', 'libx264', 'yuv420p', ['-preset', 'medium', '-f', 'mpegts']),
    ('MOVH264.MOV', 'libx264', 'yuv420p', ['-preset', 'medium']),
]
records = []
for name, codec, pixfmt, extra in formats:
    subprocess.run(['ffmpeg', '-v', 'error', '-f', 'rawvideo', '-pix_fmt', 'yuv420p',
        '-video_size', '64x48', '-framerate', '25', '-i', str(source), '-an',
        '-c:v', codec, '-pix_fmt', pixfmt, '-threads', '1', *extra, '-y', str(out/name)], check=True)
    filters = ['-vf', 'scale=in_range=pc:out_range=tv'] if name == 'MJPEG.AVI' else []
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(out/name),
        *filters, '-pix_fmt', 'yuv420p', '-f', 'rawvideo', '-'])
    if len(raw) != 12*4608:
        raise ValueError(name+': expected twelve 64x48 I420 frames')
    hashes = []
    for offset in range(0, len(raw), 4608):
        value = 2166136261
        for byte in raw[offset:offset+4608]:
            value = ((value^byte)*16777619)&0xffffffff
        hashes.append(value)
    if len(set(hashes)) != 12:
        raise ValueError(name+': fixture must have twelve distinct frames')
    reference = Path(name).stem+'.YUV'
    (out/reference).write_bytes(raw)
    records.append({'file': name, 'codec': codec, 'frames': 12, 'hashes': hashes,
        'sha256': hashlib.sha256((out/name).read_bytes()).hexdigest(),
        'reference': reference, 'reference_sha256': hashlib.sha256(raw).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records, indent=2)+'\n')
(out/'HOSTVER.TXT').write_bytes(subprocess.check_output(['ffmpeg', '-version']))
print('Generated '+str(len(records))+' video/container cases')
