#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/CXXTEST
flags='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=0x0600 -DRP_LEGACY=1'
sh GPP386.SH $flags -std=c++17 -shared -static-libgcc -static-libstdc++ -o BUILD/CXXTEST/CXXWORK.DLL TESTS/CXXWORK.CPP \
  -Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp \
  -Wl,--whole-archive,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,/workspace/RPLIB/lib/net.a,--no-whole-archive
cp BUILD/CXXTEST/CXXWORK.DLL BUILD/CXXRAW.DLL
i686-w64-mingw32-gcc -x c -Os -march=i386 -fno-builtin -fno-stack-protector \
  -D_WIN32_WINNT=0x0400 -c TESTS/CXXTRY.C -o BUILD/CXXTRY.O
i686-w64-mingw32-gcc -nostdlib -o BUILD/CXXTEST/CXXTRY.EXE BUILD/CXXTRY.O \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--no-insert-timestamp -lkernel32 -luser32
cp /workspace/RPLIB/bin/*.DLL BUILD/CXXTEST/
cp BUILD/ATTEST/ATTEST.EXE BUILD/ATTEST/ATWORK.DLL BUILD/KTEST/KTEST.EXE \
  BUILD/CRTEST/CRTEST.EXE BUILD/SYNC/SYNCTEST.EXE BUILD/CXXTEST/
i686-w64-mingw32-gcc -x c -Os -march=i386 -fno-builtin -fno-stack-protector \
  -D_WIN32_WINNT=0x0400 -DCXX_ONLY -c TESTS/SUITE.C -o BUILD/CXXSUITE.O
i686-w64-mingw32-gcc -nostdlib -o BUILD/CXXTEST/CXXSUITE.EXE BUILD/CXXSUITE.O \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--no-insert-timestamp -lkernel32 -luser32
i686-w64-mingw32-strip --strip-unneeded BUILD/CXXTEST/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/CXXTEST
