#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
for mode in current old; do
 case "$mode" in current) name=RESAMP; define= ;; old) name=RESOLD; define=-DRP_RESAMP_OLD=1 ;; esac
./GCC386.SH $define -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll \
 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -DMODULE_NAME=resamp -DMODULE_STRING='"resamp"' \
 -I/workspace/ASMBLD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -c TESTS/RESAMP.C -o BUILD/$name.O
./GCC386.SH -mcrtdll=crtdll -static-libgcc -o BUILD/ASM/$name.EXE BUILD/$name.O \
 -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
 -Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
 -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp
done
