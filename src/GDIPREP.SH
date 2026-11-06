#!/bin/sh
# Build prepared GDI presentation separately from the accepted ASM payload.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
make -C /workspace/ASMBLD/modules -j2 librpgdi_plugin.la
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/GDIPREP/PLUGINS rpgdi
i686-w64-mingw32-strip --strip-unneeded BUILD/GDIPREP/PLUGINS/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/GDIPREP > RESULTS/GDIPREP.JSN
