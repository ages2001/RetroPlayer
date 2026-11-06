"""Decode each chained Opus logical stream with a fresh host decoder."""
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root/'BUILD/AFMEDIA'
records = json.loads((out/'REFS.JSN').read_text())
for item in records:
    if item['file'] != 'OPCHAIN.OPU':
        continue
    data = (out/item['file']).read_bytes()
    starts = []
    offset = 0
    while offset < len(data):
        if data[offset:offset+4] != b'OggS' or offset+27 > len(data):
            raise ValueError('Invalid owned Ogg fixture')
        if data[offset+5] & 2:
            starts.append(offset)
        segments = data[offset+26]
        offset += 27+segments+sum(data[offset+27:offset+27+segments])
    if offset != len(data) or len(starts) != 2 or starts[0] != 0:
        raise ValueError('Expected two complete logical streams')
    starts.append(len(data))
    reference = bytearray()
    with tempfile.TemporaryDirectory(prefix='rp-opus-') as scratch:
        link = Path(scratch)/'LINK.OPU'
        for begin, end in zip(starts, starts[1:]):
            link.write_bytes(data[begin:end])
            reference.extend(subprocess.check_output(['ffmpeg', '-v', 'error', '-i', str(link),
                '-ac', '1', '-ar', '48000', '-f', 's16le', '-']))
    (out/item['reference']).write_bytes(reference)
    item['reference_decoder'] = 'host FFmpeg; fresh decoder for each logical stream'
    item['reference_sha256'] = hashlib.sha256(reference).hexdigest()
    item['samples'] = len(reference)//2
(out/'REFS.JSN').write_text(json.dumps(records, indent=2)+'\n')
