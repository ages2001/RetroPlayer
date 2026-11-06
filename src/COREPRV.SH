#!/bin/sh
# Build initial-preview preservation with the condition-event/cold-clock fixes.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/COREPRV
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/COREPRV/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/COREPRV/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/COREPRV > RESULTS/COREPRV.JSN
