#!/bin/sh
# Diagnostic TSC/clock/cursor sampling against the preserved WPROBE candidate.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import subprocess
source=Path('/workspace/VLCWORK/modules/audio_output/waveout.c')
saved=source.read_bytes()
if saved!=Path('SOURCES/WPROBE.C').read_bytes():
    raise SystemExit('Diagnostic requires the preserved WPROBE source')
try:
    source.write_bytes(Path('SOURCES/WCTSC.C').read_bytes())
    subprocess.run(['make','-C','/workspace/ASMBLD/modules','-j2','libwaveout_plugin.la'],check=True)
    subprocess.run(['python3','SCRIPTS/PLUGINS.PY','/workspace/ASMBLD/modules/.libs',
                    'BUILD/WCTSC/PLUGINS','waveout'],check=True)
    subprocess.run(['i686-w64-mingw32-strip','--strip-unneeded',
                    'BUILD/WCTSC/PLUGINS/P88D65FB.DLL'],check=True)
finally:
    source.write_bytes(saved)
PY
python3 SCRIPTS/AUDIT.PY BUILD/WCTSC > RESULTS/WCTSC.JSN
