#!/bin/sh
# Preserve the original first-frame diagnostic and build a separate fixture set.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/AV2
if test ! -f BUILD/AV2MEDIA/REFS.JSN; then
 python3 SCRIPTS/AVMEDIA.PY --lead
fi
python3 - <<'PY'
from pathlib import Path
import hashlib,json
base=Path('BUILD/AV2MEDIA')
for entry in json.loads((base/'REFS.JSN').read_text()):
    if hashlib.sha256((base/entry['file']).read_bytes()).hexdigest()!=entry['sha256']:
        raise SystemExit('AV2 fixture checksum failed: '+entry['file'])
PY
i686-w64-mingw32-gcc -DRP_AV_LEAD=1 -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector -nostdlib \
 -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
 -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -o BUILD/AV2/AV2TEST.EXE TESTS/AVTEST.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32 -lgdi32
