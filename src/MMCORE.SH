#!/bin/sh
# Keep first-picture/condition fixes and restore upstream default clock choice.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/MMCORE
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/MMCORE/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/MMCORE/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/MMCORE > RESULTS/MMCORE.JSN
