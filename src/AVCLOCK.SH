#!/bin/sh
# First-picture/audio timestamp diagnostic against the preserved TSC core.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import subprocess
files={'DEC':'src/input/decoder.c','VOUT':'src/video_output/video_output.c',
       'AOUT':'src/audio_output/dec.c'}
saved={}
for label,name in files.items():
    source=Path('/workspace/VLCWORK')/name
    saved[name]=source.read_bytes()
    if saved[name]!=(Path('SOURCES')/(label+'TSC.C')).read_bytes():
        raise SystemExit('Diagnostic source checkpoint changed: '+name)
try:
    for label,name in files.items():
        (Path('/workspace/VLCWORK')/name).write_bytes(
            (Path('SOURCES')/('AV'+label+'.C')).read_bytes())
    subprocess.run(['make','-C','/workspace/ASMBLD/src','-j2'],check=True)
    output=Path('BUILD/AVCLOCK');output.mkdir(exist_ok=True)
    (output/'LIBCORE.DLL').write_bytes(Path('/workspace/ASMBLD/src/.libs/libcore.dll').read_bytes())
    subprocess.run(['i686-w64-mingw32-strip','--strip-unneeded',str(output/'LIBCORE.DLL')],check=True)
finally:
    for name,data in saved.items():
        (Path('/workspace/VLCWORK')/name).write_bytes(data)
PY
python3 SCRIPTS/AUDIT.PY BUILD/AVCLOCK > RESULTS/AVCLOCK.JSN
