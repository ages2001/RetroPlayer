#!/bin/sh
# Verify the exact VLC-contrib Speex and SpeexDSP release archives.
set -eu
cd "$(dirname "$0")"
python3 - <<'PY'
from pathlib import Path
import hashlib,json,subprocess
for item in json.loads(Path('SOURCES/SPEEX.JSN').read_text()):
    source=Path('SOURCES')/item['archive'];data=source.read_bytes()
    if hashlib.sha256(data).hexdigest()!=item['sha256'] or hashlib.sha512(data).hexdigest()!=item['sha512']:
        raise SystemExit('Speex source checksum mismatch: '+str(source))
    work=Path('/workspace')/item['work']
    if not work.exists():
        work.mkdir()
        subprocess.run(['tar','xf',str(source),'-C',str(work),'--strip-components=1'],check=True)
PY
