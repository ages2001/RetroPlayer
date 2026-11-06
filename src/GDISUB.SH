#!/bin/sh
# Candidate-only GDI subtitle regression; preserve the accepted payload.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/GDISUB
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector -nostdlib \
 -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -o BUILD/GDISUB/GDISUB.EXE TESTS/GDISUB.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32 -lgdi32
python3 SCRIPTS/AUDIT.PY BUILD/GDISUB > RESULTS/GDISUB.JSN
