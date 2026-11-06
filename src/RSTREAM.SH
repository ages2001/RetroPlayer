#!/bin/sh
# Build the stateful resampler and compare it with the previous fixed and
# untouched upstream sources. Keep the accepted ASM checkpoint unchanged.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/RSTREAM
make -C /workspace/ASMBLD/modules -j2 libugly_resampler_plugin.la
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/RSTREAM/PLUGINS ugly_resampler
i686-w64-mingw32-strip --strip-unneeded BUILD/RSTREAM/PLUGINS/*.DLL
for mode in current base old div; do
 case "$mode" in
 current) name=RESAMP; define= ;;
 base) name=RESBASE; define=-DRP_RESAMP_BASE=1 ;;
 old) name=RESOLD; define=-DRP_RESAMP_OLD=1 ;;
 div) name=RSDIV; define=-DRP_RESAMP_DIV=1 ;;
 esac
 ./GCC386.SH $define -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -DMODULE_NAME=resamp -DMODULE_STRING='"resamp"' \
  -I/workspace/ASMBLD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
  -c TESTS/RESAMP.C -o BUILD/RSTREAM/$name.O
 ./GCC386.SH -mcrtdll=crtdll -static-libgcc -o BUILD/RSTREAM/$name.EXE BUILD/RSTREAM/$name.O \
  -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
  -Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
  -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp
done
python3 SCRIPTS/AUDIT.PY BUILD/RSTREAM > RESULTS/RSTREAM.JSN
