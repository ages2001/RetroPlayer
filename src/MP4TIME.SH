#!/bin/sh
# Build the edited-MP4 presentation-time candidate independently of ASM.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
make -C /workspace/ASMBLD/modules -j2 libmp4_plugin.la
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/MP4TIME/PLUGINS mp4
i686-w64-mingw32-strip --strip-unneeded BUILD/MP4TIME/PLUGINS/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/MP4TIME > RESULTS/MP4TIME.JSN
