#!/bin/sh
set -eu
cd "$(dirname "$0")"
(cd SOURCES && sha256sum -c FONT.SHA)
python3 - <<'PY'
import hashlib, json, subprocess
from pathlib import Path
for entry in json.loads(Path('SOURCES/DEPS.JSN').read_text()):
    archive=Path('SOURCES')/entry['archive']
    with archive.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=entry['sha256']:
            raise SystemExit('Dependency archive hash mismatch: '+str(archive))
    dest=Path('/workspace')/entry['work']
    if not dest.exists():
        dest.mkdir()
        subprocess.run(['tar','-xf',str(archive),'-C',str(dest),
                        '--strip-components='+str(entry['strip'])],check=True)
PY
patch_file="$PWD/PATCHES/LIBXML.DIF"
if patch --batch --forward --dry-run -p1 -d /workspace/XMLWORK < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 -d /workspace/XMLWORK < "$patch_file"
elif ! patch --batch --reverse --dry-run -p1 -d /workspace/XMLWORK < "$patch_file" >/dev/null 2>&1; then
  echo 'Unexpected libxml2 changes; preserving them for review' >&2
  exit 2
fi
patch_file="$PWD/PATCHES/FREETYP.DIF"
if patch --batch --forward --dry-run -p1 -d /workspace/FTWORK < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 -d /workspace/FTWORK < "$patch_file"
elif ! patch --batch --reverse --dry-run -p1 -d /workspace/FTWORK < "$patch_file" >/dev/null 2>&1; then
  echo 'Unexpected FreeType changes; preserving them for review' >&2
  exit 2
fi
