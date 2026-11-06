"""Verify Vorbis reference lengths with a host libvorbisfile decoder."""
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root/'BUILD/AFMEDIA'
binary = root/'BUILD/VORREF.BIN'
subprocess.run(['cc', '-x', 'c', '-Wall', '-Wextra', '-Werror', str(root/'TESTS/VORREF.C'),
                '-I/workspace/VORWORK/include', '-I/workspace/OGWORK/include',
                '-I/workspace/OGBLD/include', '-Wl,-l:libvorbisfile.so.3',
                '-Wl,-l:libvorbis.so.0', '-Wl,-l:libogg.so.0', '-o', str(binary)], check=True)
records = json.loads((out/'REFS.JSN').read_text())
for item in records:
    if not item['file'].endswith('.OGG'):
        continue
    raw = subprocess.check_output([str(binary), str(out/item['file'])])
    (out/item['reference']).write_bytes(raw)
    item['reference_decoder'] = 'host libvorbisfile; explicit seek to sample zero'
    item['samples'] = len(raw)//2
    item['reference_sha256'] = hashlib.sha256(raw).hexdigest()
(out/'REFS.JSN').write_text(json.dumps(records, indent=2)+'\n')
