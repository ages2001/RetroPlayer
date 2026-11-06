#!/bin/sh
# Diagnostic-only waveOut timestamps; restore the working playback source.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import subprocess
import os
source=Path('/workspace/VLCWORK/modules/audio_output/waveout.c')
trace=Path(os.environ.get('RP_WAVE_TRACE_SRC','SOURCES/WTRACE.C'))
output=Path(os.environ.get('RP_WAVE_TRACE_OUT','BUILD/WTRACE'))
saved=source.read_bytes()
if saved!=Path('SOURCES/WLAT2.C').read_bytes():
    raise SystemExit('Diagnostic source is for the preserved measured-latency candidate only')
try:
    source.write_bytes(trace.read_bytes())
    subprocess.run(['make','-C','/workspace/ASMBLD/modules','-j2','libwaveout_plugin.la'],check=True)
    subprocess.run(['python3','SCRIPTS/PLUGINS.PY','/workspace/ASMBLD/modules/.libs',
                    str(output/'PLUGINS'),'waveout'],check=True)
    subprocess.run(['i686-w64-mingw32-strip','--strip-unneeded',
                    str(output/'PLUGINS/P88D65FB.DLL')],check=True)
finally:
    source.write_bytes(saved)
PY
python3 SCRIPTS/AUDIT.PY "${RP_WAVE_TRACE_OUT:-BUILD/WTRACE}"
