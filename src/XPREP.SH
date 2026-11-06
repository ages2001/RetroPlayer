#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
from pathlib import Path
import hashlib,json,subprocess
for item in json.loads(Path('SOURCES/XIPH.JSN').read_text()):
    archive=Path('SOURCES')/item['archive']
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=item['sha256']:
        raise SystemExit('Xiph archive checksum failed: '+str(archive))
    dest=Path('/workspace')/item['work']
    if not dest.exists():
        dest.mkdir()
        subprocess.run(['tar','xf',str(archive),'-C',str(dest),
                        '--strip-components='+str(item['strip'])],check=True)
Path('/workspace/OPWORK/package_version').write_text('AUTO_UPDATE=no\nPACKAGE_VERSION="1.6.1"\n')
PY
if patch --batch --forward --dry-run -p1 -d /workspace/VORWORK < PATCHES/VORBIS.DIF >/dev/null 2>&1; then
 patch --batch --forward -p1 -d /workspace/VORWORK < PATCHES/VORBIS.DIF
else
 patch --batch --reverse --dry-run -p1 -d /workspace/VORWORK < PATCHES/VORBIS.DIF >/dev/null
fi
if patch --batch --forward --dry-run -p1 -d /workspace/FLWORK < PATCHES/FLAC.DIF >/dev/null 2>&1; then
 patch --batch --forward -p1 -d /workspace/FLWORK < PATCHES/FLAC.DIF
else
 patch --batch --reverse --dry-run -p1 -d /workspace/FLWORK < PATCHES/FLAC.DIF >/dev/null
fi
if [ ! -f /workspace/OPWORK/configure ]; then
 (cd /workspace/OPWORK && autoreconf -isf)
fi
for pair in OG:OGG VOR:VORBIS FL:FLAC OP:OPUS; do
 source=${pair%:*}; name=${pair#*:}
 if [ "$source" = FL ]; then file=COPYING.Xiph; else file=COPYING; fi
 cp "/workspace/${source}WORK/$file" "LICENSES/$name.TXT"
done
