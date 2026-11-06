#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh ATTEST.SH
sh SYNC.SH
sh CRT.SH
sh TLS.SH
sh NET.SH
mkdir -p BUILD/SUITE
for suite in ATTEST KTEST CRTEST TLTEST NETTEST; do
  cp "BUILD/$suite/"*.EXE "BUILD/$suite/"*.DLL BUILD/SUITE/
done
cp BUILD/SYNC/*.EXE BUILD/SYNC/*.DLL BUILD/SUITE/
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -c TESTS/SUITE.C -o BUILD/SUITE.O
i686-w64-mingw32-gcc -nostdlib -o BUILD/SUITE/SUITE.EXE BUILD/SUITE.O \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
python3 SCRIPTS/AUDIT.PY BUILD/SUITE
