#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
import hashlib,json,os,shutil,subprocess
from pathlib import Path
out=Path('BUILD/VFMEDIA');base=Path('BUILD/ASM');manifest=base/'VFREFS.JSN'
out.mkdir(parents=True,exist_ok=True)
if os.environ.get('RP_VF_REGEN')=='1' or not manifest.exists():
    subprocess.run(['python3','SCRIPTS/VFMEDIA.PY'],check=True)
    records=json.loads((out/'REFS.JSN').read_text())
else:
    records=json.loads(manifest.read_text())
    for item in records:
        for key,check in [('file','sha256'),('reference','reference_sha256')]:
            path=base/item[key]
            if hashlib.sha256(path.read_bytes()).hexdigest()!=item[check]:
                raise SystemExit('Video fixture checksum failed: '+str(path))
            shutil.copyfile(path,out/item[key])
header=['#define RP_VIDEO_FILES '+str(len(records)),
        'static const char *media_names[]={'+','.join('"'+r['file']+'"' for r in records)+'};',
        'static const char *reference_names[]={'+','.join('"'+r['reference']+'"' for r in records)+'};',
        'static const unsigned long frame_hashes[][12]={']
header+=['{'+','.join('0x%08xUL'%h for h in r['hashes'])+'},' for r in records]
header+=['};']
for line in Path('BUILD/MEDDATA.H').read_text().splitlines():
    if line.startswith('#define AAC_'):header.append(line)
Path('BUILD/VFDATA.H').write_text('\n'.join(header)+'\n')
for item in records:
    for key in ['file','reference']:shutil.copyfile(out/item[key],base/item[key])
manifest.write_text(json.dumps(records,indent=2)+'\n')
PY
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
 -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
 -fno-builtin -fno-stack-protector -nostdlib -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
 -DRP_CODEC_THREADS=2 -DRP_WITH_AV1=0 -DRP_USE_CACHE=1 -DRP_AUTO_DEMUX=1 -DRP_VIDEO_DUMP=1 -DRP_PIXEL_CHECK=1 \
 '-DRP_MEDIA_HEADER="VFDATA.H"' '-DRP_REPORT_NAME="VFMTEST.TXT"' '-DRP_LOG_NAME="VFMLOG.TXT"' \
 -IBUILD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -o BUILD/ASM/VFMTEST.EXE TESTS/MEDTEST.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32
