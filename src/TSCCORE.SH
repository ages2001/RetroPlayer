#!/bin/sh
# Calibrated, CPUID-gated QEMU clock plus the shared first-output lead.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/TSCCORE
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/TSCCORE/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/TSCCORE/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/TSCCORE > RESULTS/TSCCORE.JSN
