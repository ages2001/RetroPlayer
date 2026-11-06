#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/ASM
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll \
 -I/workspace/ASMLIB/include -c TESTS/CPUTEST.C -o BUILD/CPUTEST.O
./GCC386.SH -mcrtdll=crtdll -static-libgcc -o BUILD/ASM/CPUTEST.EXE BUILD/CPUTEST.O \
 -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
 /workspace/ASMLIB/lib/libavutil.a \
 -Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
 -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp
i686-w64-mingw32-strip --strip-unneeded BUILD/ASM/CPUTEST.EXE
