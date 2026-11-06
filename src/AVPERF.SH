#!/bin/sh
# Isolate clock source from timer resolution in the owned AV2 diagnostic.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/AVPERF
i686-w64-mingw32-gcc -DRP_AV_LEAD=1 -DRP_AV_PERF=1 -x c -std=c99 -O2 \
 -Wall -Wextra -Werror -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
 -fno-builtin -fno-stack-protector -nostdlib -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
 -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -o BUILD/AVPERF/AVPERF.EXE TESTS/AVTEST.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
 -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp \
 -lkernel32 -luser32 -lgdi32 -lwinmm -lgcc
