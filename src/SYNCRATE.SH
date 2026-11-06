#!/bin/sh
# Scale audio drift correction by media time instead of packet count.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/SYNCRATE
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/SYNCRATE/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/SYNCRATE/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/SYNCRATE > RESULTS/SYNCRATE.JSN
