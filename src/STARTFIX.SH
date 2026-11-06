#!/bin/sh
# Build normal first-picture clocking and Win9x clock selection as a candidate.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/STARTFIX
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/STARTFIX/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/STARTFIX/LIBCORE.DLL
python3 SCRIPTS/AUDIT.PY BUILD/STARTFIX > RESULTS/STARTFIX.JSN
