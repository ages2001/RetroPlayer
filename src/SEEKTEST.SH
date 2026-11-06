#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 SCRIPTS/SEEKDATA.PY
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
 -mcrtdll=crtdll -IBUILD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -c TESTS/SEEKTEST.C -o BUILD/SEEKTEST.O
./GCC386.SH -nostdlib -mcrtdll=crtdll -o BUILD/ASM/SEEKTEST.EXE BUILD/SEEKTEST.O \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp \
 -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
 -lmingwex -lcrtdll -lkernel32 -luser32 -lgcc
cp BUILD/SAMPLES/SEEK.MP4 BUILD/ASM/
