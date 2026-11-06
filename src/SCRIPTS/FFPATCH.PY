"""Record the legacy codec changes against the pinned FFmpeg release."""
import difflib
from pathlib import Path
import subprocess

reference=Path('/workspace/FFREF')
working=Path('/workspace/FFWORK')
project=Path(__file__).resolve().parent.parent
revision='38b88335f99e76ed89ff3c93f877fdefce736c13'
if subprocess.check_output(['git','-C',str(reference),'rev-parse','HEAD'],text=True).strip()!=revision:
    raise SystemExit('Unexpected FFmpeg revision')
if subprocess.check_output(['git','-C',str(reference),'status','--porcelain']):
    raise SystemExit('Modified FFmpeg reference')
patch=[]
for name in ['configure','libavutil/wchar_filename.h','compat/w32pthreads.h','libavformat/udp.c',
             'libavutil/x86/cpu.c']:
    patch.extend(difflib.unified_diff((reference/name).read_text().splitlines(keepends=True),
        (working/name).read_text().splitlines(keepends=True),fromfile='a/'+name,tofile='b/'+name))
(project/'PATCHES'/'FFMPEG.DIF').write_text(''.join(patch))
print('Recorded FFmpeg changes against '+revision)
