"""Generate an owned GOP/B-frame fixture and independent decoded frame hashes."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
out=root/'BUILD/SAMPLES';out.mkdir(exist_ok=True)
raw=b''.join(bytes([16+3*n])*3072+bytes([128])*1536 for n in range(60))
common=['ffmpeg','-hide_banner','-loglevel','error']
subprocess.run(common+['-f','rawvideo','-pixel_format','yuv420p','-video_size','64x48',
    '-framerate','2','-i','pipe:0','-an','-c:v','libx264','-qp','12','-g','10',
    '-keyint_min','10','-sc_threshold','0','-bf','2','-threads','1','-y',str(out/'SEEK.MP4')],
    input=raw,check=True)
decoded=subprocess.check_output(common+['-i',str(out/'SEEK.MP4'),'-pix_fmt','yuv420p','-f','rawvideo','-'])
assert len(decoded)==60*4608
hashes=[]
for offset in range(0,len(decoded),4608):
    value=2166136261
    for byte in decoded[offset:offset+4608]:value=((value^byte)*16777619)&0xffffffff
    hashes.append(value)
assert len(set(hashes))==60
(root/'BUILD/SEEKDATA.H').write_text('/* Independent host-decoded H.264 frame hashes. */\n'
    'static const unsigned long reference[60]={\n'+',\n'.join(f'0x{x:08x}UL' for x in hashes)+'\n};\n')
print('Created 30-second H.264 seek fixture with 60 unique reference frames')
