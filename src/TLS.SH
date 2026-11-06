#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/TLTEST
for unit in TLWORK TLTEST; do
  i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
    -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
    -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
    -c "TESTS/$unit.C" -o "BUILD/$unit.O"
done
i686-w64-mingw32-gcc -shared -nostdlib -o BUILD/TLTEST/TLWORK.DLL BUILD/TLWORK.O \
  /workspace/RPLIB/lib/crt.a -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp,--out-implib,BUILD/TLWORK.A -lkernel32
i686-w64-mingw32-gcc -nostdlib -o BUILD/TLTEST/TLTEST.EXE BUILD/TLTEST.O \
  BUILD/TLWORK.A /workspace/RPLIB/lib/crt.a \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
cp /workspace/RPLIB/bin/RPCRT.DLL /workspace/RPLIB/bin/RPKERN.DLL BUILD/TLTEST/
python3 SCRIPTS/AUDIT.PY BUILD/TLTEST
