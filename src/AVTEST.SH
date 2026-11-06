#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import hashlib,json,os,shutil,subprocess
base=Path('BUILD/ASM'); manifest=base/'AVREFS.JSN'; source=Path('BUILD/AVMEDIA')
if os.environ.get('RP_AV_REGEN')=='1' or not manifest.exists():
    subprocess.run(['python3','SCRIPTS/AVMEDIA.PY'],check=True)
    shutil.copyfile(source/'REFS.JSN',manifest)
    for item in json.loads(manifest.read_text()):shutil.copyfile(source/item['file'],base/item['file'])
for item in json.loads(manifest.read_text()):
    if hashlib.sha256((base/item['file']).read_bytes()).hexdigest()!=item['sha256']:
        raise SystemExit('AV fixture checksum failed: '+item['file'])
PY
for mode in native avformat fixed pcm; do
 case "$mode" in native) output=AVTEST; define= ;; avformat) output=AVFTEST; define=-DRP_AV_FORMAT=1 ;;
 fixed) output=AVFIX; define=-DRP_AV_FIXED=1 ;; pcm) output=AVPCM; define=-DRP_AV_PCM=1 ;; esac
i686-w64-mingw32-gcc $define -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector -nostdlib \
 -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
 -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -o BUILD/ASM/$output.EXE TESTS/AVTEST.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32 -lgdi32
done
