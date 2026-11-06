#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector -nostdlib \
 -o BUILD/BLTTEST.EXE TESTS/BLTTEST.C -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32 -lgdi32 -lgcc
