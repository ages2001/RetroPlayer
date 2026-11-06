#!/bin/sh
# Reject a consistently mis-scaled QPC calibration before selecting TSC.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/TSCGUARD
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/TSCGUARD/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/TSCGUARD/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/TSCGUARD > RESULTS/TSCGUARD.JSN
