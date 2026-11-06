#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
# Reuse the packaged, hashed fixtures unless regeneration is requested.
python3 - <<'PYSET'
import hashlib,json,os,shutil,subprocess
from pathlib import Path
base=Path('BUILD/ASM');manifest=base/'AFREFS.JSN'
if os.environ.get('RP_AF_REGEN')=='1' or not manifest.exists():
    subprocess.run(['python3','SCRIPTS/AFMEDIA.PY'],check=True)
    subprocess.run(['python3','SCRIPTS/XMEDIA.PY'],check=True)
    subprocess.run(['python3','SCRIPTS/VORREF.PY'],check=True)
    subprocess.run(['python3','SCRIPTS/XREFS.PY'],check=True)
else:
    entries=json.loads(manifest.read_text());out=Path('BUILD/AFMEDIA');out.mkdir(exist_ok=True)
    for item in entries:
        for key,check in [('file','sha256'),('reference','reference_sha256')]:
            source=base/item[key]
            if hashlib.sha256(source.read_bytes()).hexdigest()!=item[check]:
                raise SystemExit('Audio fixture checksum failed: '+str(source))
            shutil.copyfile(source,out/item[key])
    shutil.copyfile(manifest,out/'REFS.JSN')
entries=json.loads(Path('BUILD/AFMEDIA/REFS.JSN').read_text())
Path('BUILD/AFDATA.H').write_text('static const struct {const char *file,*raw;} cases[]={\n'+
    ''.join('{"'+r['file']+'","'+r['reference']+'"},\n' for r in entries)+'};\n')
PYSET
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
 -mcrtdll=crtdll -IBUILD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -c TESTS/AFMTEST.C -o BUILD/AFMTEST.O
./GCC386.SH -nostdlib -mcrtdll=crtdll -o BUILD/ASM/AFMTEST.EXE BUILD/AFMTEST.O \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp \
 -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
 -lmingwex -lcrtdll -lkernel32 -luser32 -lgcc
python3 - <<'PY'
import json,shutil
from pathlib import Path
root=Path('BUILD/AFMEDIA')
for item in json.loads((root/'REFS.JSN').read_text()):
    for key in ('file','reference'):shutil.copyfile(root/item[key],Path('BUILD/ASM')/item[key])
shutil.copyfile(root/'REFS.JSN','BUILD/ASM/AFREFS.JSN')
PY
