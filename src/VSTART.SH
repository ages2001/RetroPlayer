#!/bin/sh
# Build the first-picture synchronization candidate without changing ASM.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
make -C /workspace/ASMBLD/src -j2
mkdir -p BUILD/VSTART
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/VSTART/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/VSTART/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/VSTART > RESULTS/VSTART.JSN
