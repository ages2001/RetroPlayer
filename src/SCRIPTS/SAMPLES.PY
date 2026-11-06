"""Generate owned synthetic media and independent host-decoder references."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parent.parent
output=root/'BUILD'/'SAMPLES'
output.mkdir(parents=True,exist_ok=True)

def ff(*args):
    return subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error',*args])

source=['-f','lavfi','-i','testsrc2=size=64x48:rate=6:duration=2','-an']
for name,codec,extra in [
    ('H264.MP4','libx264',['-preset','medium','-crf','22']),
    ('HEVC.MKV','libx265',['-preset','fast','-x265-params','pools=none:frame-threads=1:log-level=error']),
    ('VP9.WEB','libvpx-vp9',['-f','webm']),
    ('AV1.MKV','libaom-av1',['-cpu-used','6','-crf','35','-b:v','0'])]:
    ff(*source,'-c:v',codec,'-pix_fmt','yuv420p','-threads','1',*extra,'-y',str(output/name))
ff('-i',str(output/'H264.MP4'),'-c','copy','-y',str(output/'H264.MKV'))
ff('-f','lavfi','-i','sine=frequency=440:sample_rate=48000:duration=2',
   '-c:a','aac','-b:a','96k','-threads','1','-y',str(output/'AAC.M4A'))

lines=['/* Independently decoded by the host FFmpeg, not the target library. */',
       'static const char *media_names[]={"H264.MP4","H264.MKV","HEVC.MKV","VP9.WEB","AV1.MKV"};',
       'static const unsigned long frame_hashes[][12]={']
for name in ['H264.MP4','H264.MKV','HEVC.MKV','VP9.WEB','AV1.MKV']:
    raw=ff('-i',str(output/name),'-pix_fmt','yuv420p','-f','rawvideo','-')
    assert len(raw)==64*48*3//2*12
    hashes=[]
    for offset in range(0,len(raw),4608):
        value=2166136261
        for byte in raw[offset:offset+4608]: value=((value^byte)*16777619)&0xffffffff
        hashes.append(f'0x{value:08x}UL')
    lines.append('    {'+','.join(hashes)+'},')
lines.append('};')
raw=ff('-i',str(output/'AAC.M4A'),'-ac','1','-ar','48000','-f','s16le','-')
import struct
samples=struct.unpack('<'+'h'*(len(raw)//2),raw)
lines += [f'#define AAC_SAMPLES {len(samples)}U',
          f'#define AAC_ABSUM {sum(abs(sample) for sample in samples)}ULL']
(root/'BUILD'/'MEDDATA.H').write_text('\n'.join(lines)+'\n')
(output/'HOSTVER.TXT').write_bytes(subprocess.check_output(['ffmpeg','-version']))
print('Generated five video fixtures and AAC reference:',len(samples),'samples')
