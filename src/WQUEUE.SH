#!/bin/sh
# Build a bounded waveOut queue candidate; leave validated ASM unchanged.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
make -C /workspace/ASMBLD/modules -j2 libwaveout_plugin.la
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/WQUEUE/PLUGINS waveout
i686-w64-mingw32-strip --strip-unneeded BUILD/WQUEUE/PLUGINS/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/WQUEUE > RESULTS/WQUEUE.JSN
