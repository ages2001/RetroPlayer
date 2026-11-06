#!/bin/sh
# Reproduce the archived native-gate experiment; not the accepted runtime.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/GATE
test -f BUILD/ATOM.DEF
test -f BUILD/KERN.DEF
for unit in ATOM KERN UTF8 SYNC MODULE FIND; do
 case "$unit" in
  ATOM) source=SOURCES/ATGATE2.C ;; KERN) source=SOURCES/KEGATE2.C ;;
  SYNC) source=SOURCES/SYGATE2.C ;; *) source=SRC/$unit.C ;;
 esac
 i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -Wno-cast-function-type -march=i386 -mtune=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
  -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -ISRC -c "$source" -o "BUILD/GATE/$unit.O"
done
i686-w64-mingw32-gcc -shared -nostdlib -o BUILD/GATE/RPATOM.DLL \
 BUILD/GATE/ATOM.O BUILD/ATOM.DEF -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32
i686-w64-mingw32-gcc -shared -nostdlib -o BUILD/GATE/RPKERN.DLL \
 BUILD/GATE/KERN.O BUILD/GATE/UTF8.O BUILD/GATE/SYNC.O BUILD/GATE/MODULE.O BUILD/GATE/FIND.O \
 BUILD/KERN.DEF -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32
RP_PRI_OUT=BUILD/GATE RP_PRI_GATE=1 sh PRITEST.SH
python3 SCRIPTS/AUDIT.PY BUILD/GATE > RESULTS/GATE.JSN
