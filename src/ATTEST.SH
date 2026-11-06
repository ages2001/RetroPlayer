#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh ATOM.SH
mkdir -p BUILD/ATTEST
for program in ATTEST ATWORK RUNTEST; do
  i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
    -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
    -fno-inline-atomics -fno-builtin -fno-stack-protector -fno-ident -include SRC/SYNCABI.H \
    -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
    -c "TESTS/$program.C" -o "BUILD/$program.O"
done
i686-w64-mingw32-gcc -shared -nostdlib -o BUILD/ATTEST/ATWORK.DLL BUILD/ATWORK.O \
  /workspace/RPLIB/lib/atom.a -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32
for program in ATTEST RUNTEST; do
i686-w64-mingw32-gcc -nostdlib -o "BUILD/ATTEST/$program.EXE" "BUILD/$program.O" \
  /workspace/RPLIB/lib/atom.a -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
done
cp /workspace/RPLIB/bin/RPATOM.DLL BUILD/ATTEST/RPATOM.DLL
python3 SCRIPTS/AUDIT.PY BUILD/ATTEST
