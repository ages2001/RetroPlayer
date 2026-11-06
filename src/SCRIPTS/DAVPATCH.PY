"""Record the dav1d port against the unmodified release used by VLC."""
import difflib
from pathlib import Path
import subprocess
reference=Path('/workspace/DAVREF')
working=Path('/workspace/DAVWORK')
revision='54706fc6bc0cdecab7e9593974a4039cc038fca7'
if subprocess.check_output(['git','-C',str(reference),'rev-parse','HEAD'],text=True).strip()!=revision:
    raise SystemExit('Unexpected dav1d revision')
if subprocess.check_output(['git','-C',str(reference),'status','--porcelain']):
    raise SystemExit('Modified dav1d reference')
patch=[]
for name in ['meson.build','meson_options.txt','src/cpu.c','src/win32/thread.c']:
    patch.extend(difflib.unified_diff((reference/name).read_text().splitlines(keepends=True),
        (working/name).read_text().splitlines(keepends=True),fromfile='a/'+name,tofile='b/'+name))
(Path(__file__).resolve().parent.parent/'PATCHES'/'DAV1D.DIF').write_text(''.join(patch))
print('Recorded dav1d changes against '+revision)
