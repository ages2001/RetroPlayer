#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh KERN.SH
mkdir -p BUILD/SYNC
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
  -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -nostdlib \
  TESTS/SYNCTEST.C -x none /workspace/RPLIB/lib/kern.a -o BUILD/SYNC/SYNCTEST.EXE \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
cp /workspace/RPLIB/bin/RPKERN.DLL BUILD/SYNC/
python3 SCRIPTS/AUDIT.PY BUILD/SYNC
