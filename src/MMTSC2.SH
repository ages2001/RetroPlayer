#!/bin/sh
# Let the legacy timer phase settle before the guarded TCG calibration.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/MMTSC2
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/MMTSC2/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/MMTSC2/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/MMTSC2 > RESULTS/MMTSC2.JSN
