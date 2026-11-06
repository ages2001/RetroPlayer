#!/bin/sh
# Build the CRT-free installer and its private real-filesystem regression.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/SETUP
mkdir -p BUILD/ASM
# The uninstaller carries the red-X icon (icon index 0) used by its Program
# Manager item on NT 3.x and Windows 3.x.
i686-w64-mingw32-windres --include-dir SRC -O coff -o BUILD/RPUNIN.RES SRC/RPUNIN.RC
for name in INSTALL RPUNIN INSTEST; do
 res=""
 case "$name" in INSTALL) source=SRC/INSTALL.C; output=BUILD/SETUP/INSTALL.EXE ;;
  RPUNIN) source=SRC/RPUNIN.C; output=BUILD/SETUP/RPUNIN.EXE; res="-x none BUILD/RPUNIN.RES" ;;
  INSTEST) source=TESTS/INSTEST.C; output=BUILD/ASM/INSTEST.EXE ;; esac
 i686-w64-mingw32-gcc -x c -std=c99 -Os -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
  -nostdlib -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -o "$output" "$source" $res \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32 -lgdi32 -ladvapi32 -lole32
done
