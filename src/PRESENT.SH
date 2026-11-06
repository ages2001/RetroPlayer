#!/bin/sh
# Build synchronous display-duration scheduling separately from ASM.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/PRESENT
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/PRESENT/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/PRESENT/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/PRESENT > RESULTS/PRESENT.JSN
