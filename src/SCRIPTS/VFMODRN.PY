"""Generate modern video fixtures (H.264, HEVC, VP9, AV1 in MP4/MKV/WebM) and
independent decoded-frame hashes. Codecs are bit-exact per spec, so the guest
engine's decode must match these host references exactly."""
import hashlib, json, subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root/'BUILD/VFMODRN'
out.mkdir(parents=True, exist_ok=True)
source = out/'SOURCE.YUV'
source.write_bytes(subprocess.check_output(['ffmpeg','-v','error','-f','lavfi','-i',
    'testsrc2=size=64x48:rate=6:duration=2','-pix_fmt','yuv420p','-f','rawvideo','-']))

# name, encoder, container-args, extra encoder args
formats = [
    ('H264MP4.MP4', 'libx264',     [],                 ['-preset','medium','-pix_fmt','yuv420p']),
    ('HEVCMP4.MP4', 'libx265',     ['-tag:v','hvc1'],  ['-preset','medium','-pix_fmt','yuv420p','-x265-params','log-level=error']),
    ('HEVCMKV.MKV', 'libx265',     [],                 ['-preset','medium','-pix_fmt','yuv420p','-x265-params','log-level=error']),
    ('VP9WEBM.WEB', 'libvpx-vp9',  ['-f','webm'],       ['-b:v','0','-crf','8','-auto-alt-ref','0','-lag-in-frames','0','-pix_fmt','yuv420p']),
    ('VP9MKV.MKV',  'libvpx-vp9',  ['-f','matroska'],   ['-b:v','0','-crf','8','-auto-alt-ref','0','-lag-in-frames','0','-pix_fmt','yuv420p']),
    ('AV1MP4.MP4',  'libaom-av1',  [],                 ['-crf','10','-cpu-used','6','-usage','realtime','-pix_fmt','yuv420p']),
    ('AV1MKV.MKV',  'libaom-av1',  ['-f','matroska'],   ['-crf','10','-cpu-used','6','-usage','realtime','-pix_fmt','yuv420p']),
    ('AV1WEBM.WEB', 'libaom-av1',  ['-f','webm'],       ['-crf','10','-cpu-used','6','-usage','realtime','-pix_fmt','yuv420p']),
]
records = []
for name, codec, cont, extra in formats:
    subprocess.run(['ffmpeg','-v','error','-f','rawvideo','-pix_fmt','yuv420p',
        '-video_size','64x48','-framerate','25','-i',str(source),'-an',
        '-c:v',codec,'-threads','1',*extra,*cont,'-y',str(out/name)], check=True)
    raw = subprocess.check_output(['ffmpeg','-v','error','-i',str(out/name),
        '-pix_fmt','yuv420p','-f','rawvideo','-'])
    if len(raw) != 12*4608:
        raise ValueError(name+': expected twelve 64x48 I420 frames, got '+str(len(raw)))
    hashes = []
    for off in range(0, len(raw), 4608):
        v = 2166136261
        for b in raw[off:off+4608]:
            v = ((v^b)*16777619)&0xffffffff
        hashes.append(v)
    if len(set(hashes)) != 12:
        raise ValueError(name+': fixture must have twelve distinct frames')
    ref = Path(name).stem+'.YUV'
    (out/ref).write_bytes(raw)
    records.append({'file':name,'codec':codec,'frames':12,'hashes':hashes,
        'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest(),
        'reference':ref,'reference_sha256':hashlib.sha256(raw).hexdigest()})
(out/'REFS.JSN').write_text(json.dumps(records,indent=2)+'\n')
print('Generated '+str(len(records))+' modern video cases')
