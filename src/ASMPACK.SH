#!/bin/sh
# Experimental full-module package; guarded FFmpeg assembly, text and cache.
# Build dependencies/core first; this is not the stable SMP checkpoint.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
RP_CORE_DIR=/workspace/ASMBLD RP_CORE_OUT=BUILD/ASM RP_CORE_MEDIA=1 \
 RP_CORE_AV1=1 RP_CORE_THREADS=2 RP_USE_CACHE=1 sh COREST.SH
RP_TEST_OUT=BUILD/ASM RP_USE_CACHE=1 sh ZTEST.SH
RP_TEST_OUT=BUILD/ASM RP_USE_CACHE=1 sh SUBTEST.SH
sh DEPTST.SH
sh CPUTEST.SH
sh FILETEST.SH
sh SEEKTEST.SH
sh UITEST.SH
sh AOUTTEST.SH
sh AFMTEST.SH
sh VFMTEST.SH
sh AVTEST.SH
sh QUTEST.SH
sh RESAMP.SH
sh PRITEST.SH
sh CACHE.SH
sh GUARD.SH
sh INSTALL.SH
sh VMAGENT.SH
cp BUILD/DEPS/DEPTST.EXE BUILD/CACHE.EXE BUILD/GUARD.EXE BUILD/VMAGENT.EXE SOURCES/DEJAVU.TTF BUILD/ASM/
python3 - <<'PY'
from pathlib import Path
import subprocess
source=Path('/workspace/ASMBLD/modules/.libs')
names=sorted(p.name[3:-11] for p in source.glob('lib*_plugin.dll'))
subprocess.run(['python3','SCRIPTS/PLUGINS.PY',str(source),'BUILD/ASM/PLUGINS']+names,check=True)
PY
i686-w64-mingw32-strip --strip-unneeded BUILD/ASM/PLUGINS/*.DLL
sh XMOD.SH
python3 SCRIPTS/INSPACK.PY
python3 SCRIPTS/AUDIT.PY BUILD/ASM
