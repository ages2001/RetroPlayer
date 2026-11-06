#!/bin/sh
set -eu
cd "$(dirname "$0")"
root=/workspace/TOOLS/SYSROOT
export PATH="$root/usr/bin:$PATH"
mkdir -p BUILD DIST
# The SDK headers require 0x0400 to parse. PE subsystem remains 3.10;
# actual imported functions are audited and tested on NT 3.1 and 95.
# RETRO carries an application icon and version resource; PROBE does not.
i686-w64-mingw32-windres --include-dir SRC -O coff -o BUILD/RETRO.RES SRC/RETRO.RC
for program in RETRO PROBE; do
i686-w64-mingw32-gcc-win32 -x c -std=c99 -Os -Wall -Wextra -Werror \
  -Wno-cast-function-type -fno-strict-aliasing \
  -march=i386 -mtune=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-builtin -fno-stack-protector -fno-ident -fno-asynchronous-unwind-tables \
  -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
  -c "SRC/$program.C" -o "BUILD/$program.O"
res=""; out="$program"
[ "$program" = RETRO ] && { res="BUILD/RETRO.RES"; out="RETROPLY"; }
i686-w64-mingw32-gcc-win32 -nostdlib -o "DIST/$out.EXE" "BUILD/$program.O" $res \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0 \
  -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp \
  -lkernel32 -luser32 -lgdi32 -lcomdlg32
done
python3 SCRIPTS/AUDIT.PY DIST
