#!/bin/sh
# Build actual condition-event integration separately from the ASM checkpoint.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/CVCORE
make -C /workspace/ASMBLD/src -j2
cp /workspace/ASMBLD/src/.libs/libcore.dll BUILD/CVCORE/LIBCORE.DLL
i686-w64-mingw32-strip --strip-unneeded BUILD/CVCORE/LIBCORE.DLL
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -march=i386 \
 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -fno-builtin -fno-stack-protector \
 -mcrtdll=crtdll -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 \
 -I/workspace/ASMBLD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -c TESTS/CWTEST.C -o BUILD/CVCORE/CWTEST.O
./GCC386.SH -nostdlib -mcrtdll=crtdll -o BUILD/CVCORE/CWTEST.EXE BUILD/CVCORE/CWTEST.O \
 /workspace/ASMBLD/src/.libs/libcore.dll.a \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
 -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp -lkernel32 -luser32 -lgcc
python3 SCRIPTS/AUDIT.PY BUILD/CVCORE > RESULTS/CVCORE.JSN
