#!/bin/sh
# Build the shared first-output preparation interval candidate separately.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/LEADCORE
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/LEADCORE/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/LEADCORE/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/LEADCORE > RESULTS/LEADCORE.JSN
