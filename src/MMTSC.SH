#!/bin/sh
# Calibrate the guarded TCG clock without the legacy HAL QPC scale.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/MMTSC
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/MMTSC/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/MMTSC/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/MMTSC > RESULTS/MMTSC.JSN
