#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
rp_pri_out=${RP_PRI_OUT:-BUILD/ASM}
mkdir -p BUILD/PRIBASE "$rp_pri_out"
python3 - <<'PY'
from pathlib import Path
for name, source, old, new in [('ATOM.C', 'ATGATE.C', 'if (prior) Sleep(1);', 'if (prior) Sleep(0);'),
                       ('SYNC.C', 'SYGATE.C', 'if(value) Sleep(1);', 'if(value) Sleep(0);')]:
    text=(Path('SOURCES')/source).read_text()
    if text.count(old)!=1:raise SystemExit('Unexpected gate implementation')
    (Path('BUILD/PRIBASE')/name).write_text(text.replace(old,new))
PY
for mode in current old; do
 case "$mode" in current) name=PRITEST; define= ;; old) name=PRIOLD; define=-DRP_PRI_OLD=1 ;; esac
 if test "$mode" = current && test "${RP_PRI_GATE:-0}" = 1; then define=-DRP_PRI_GATE=1; fi
 i686-w64-mingw32-gcc $define -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
  -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -nostdlib -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
  -o "$rp_pri_out/$name.EXE" TESTS/PRITEST.C \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
  -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp,--gc-sections -lkernel32 -luser32
done
