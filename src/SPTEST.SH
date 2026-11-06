#!/bin/sh
# Build a separate Speex candidate test without changing the ASM checkpoint.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import hashlib,json,os,subprocess
root=Path('BUILD/SPMEDIA');manifest=root/'REFS.JSN'
if os.environ.get('RP_SP_REGEN')=='1' or not manifest.exists():
    subprocess.run(['python3','SCRIPTS/SPMEDIA.PY'],check=True)
rows=json.loads(manifest.read_text())
for r in rows:
    for key,digest in [('file','sha256'),('reference','reference_sha256')]:
        if hashlib.sha256((root/r[key]).read_bytes()).hexdigest()!=r[digest]:
            raise SystemExit('Speex fixture checksum failed: '+r[key])
Path('BUILD/SPDATA.H').write_text('static const struct {const char *file,*raw;unsigned rate,channels;} cases[]={\n'+
    ''.join('{"%s","%s",%u,%u},\n'%(r['file'],r['reference'],r['rate'],r['channels']) for r in rows)+'};\n')
PY
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
 -mcrtdll=crtdll -IBUILD -I/workspace/ASMBLD/include -I/workspace/VLCWORK/include \
 -c TESTS/SPTEST.C -o BUILD/SPTEST.O
./GCC386.SH -nostdlib -mcrtdll=crtdll -o BUILD/SPTEST.EXE BUILD/SPTEST.O \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp \
 -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
 -lmingwex -lcrtdll -lkernel32 -luser32 -lgcc
