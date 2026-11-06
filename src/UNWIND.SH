#!/bin/sh
# Rebuild DWARF unwinding; the compiler's packaged library contains CMOV.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p /workspace/UNWIND /workspace/CXXLIB/lib
cd /workspace/UNWIND
/workspace/GCCWORK/libgcc/configure --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
  --disable-shared --with-cross-host=x86_64-pc-linux-gnu \
  CC=/workspace/RETRO/GCC386.SH \
  CFLAGS='-O2 -march=i386 -fno-inline-atomics -mcrtdll=crtdll'
python3 - <<'PY'
from pathlib import Path
Path('tconfig.h').write_text('#define USED_FOR_TARGET 1\n#include "auto-target.h"\n')
Path('libgcc_tm.h').write_text('/* No extra target-only headers for i386 MinGW. */\n')
p=Path('md-unwind-support.h')
if not p.exists(): p.symlink_to('/workspace/GCCWORK/libgcc/config/i386/w32-unwind.h')
PY
for rp_unit in unwind-dw2 unwind-dw2-fde unwind-sjlj unwind-c; do
 i686-w64-mingw32-gcc -O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=0x0600 \
  -D__GTHREAD_HIDE_WIN32API=1 -DIN_GCC -DIN_LIBGCC2 -fbuilding-libgcc -fno-stack-protector \
  -I. -I/workspace/TOOLS/SYSROOT/usr/lib/gcc/i686-w64-mingw32/14-win32/plugin/include \
  -I/workspace/GCCWORK/libgcc -I/workspace/GCCWORK/include \
  -c "/workspace/GCCWORK/libgcc/$rp_unit.c" -o "$rp_unit.o"
done
i686-w64-mingw32-ar rcs /workspace/CXXLIB/lib/libgcc_eh.a \
  unwind-dw2.o unwind-dw2-fde.o unwind-sjlj.o unwind-c.o
