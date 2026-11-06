#!/bin/sh
# Build a sustained silent latency probe; preserve prior candidates and ASM.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
make -C /workspace/ASMBLD/modules -j2 libwaveout_plugin.la
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/WPROBE/PLUGINS waveout
i686-w64-mingw32-strip --strip-unneeded BUILD/WPROBE/PLUGINS/P88D65FB.DLL
python3 SCRIPTS/AUDIT.PY BUILD/WPROBE > RESULTS/WPROBE.JSN
