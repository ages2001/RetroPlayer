"""Append the 16-bit player payload to the self-extracting installer stub.

Produces BUILD/SETUP/RPSET16.EXE = RPSET16 stub + payload + trailer, using the
same format as INSPACK.PY. The 16-bit payload is a flat set of DOS 8.3 files
(just RETRO16.EXE); RPSET16.EXE installs them to C:\\RETROPLY on Windows 3.x.
"""
import binascii
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
stub = root / 'BUILD/W16/RPSET16.EXE'
out = root / 'BUILD/SETUP/RPSET16.EXE'
payload_files = [('RETRO16.EXE', root / 'DIST16/RETRO16.EXE'),
                 ('RPUN16.EXE', root / 'DIST16/RPUN16.EXE')]

NAME_OK = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-')


def check_83(name):
    base, dot, ext = name.partition('.')
    if not base or len(base) > 8 or len(ext) > 3 or any(c not in NAME_OK for c in base + ext):
        raise SystemExit('Not an 8.3 name: ' + name)


blob = bytearray()
count = 0
for name, path in payload_files:
    name = name.upper()
    check_83(name)
    data = path.read_bytes()
    nb = name.encode('ascii')
    blob += struct.pack('<H', len(nb)) + nb
    blob += struct.pack('<II', len(data), binascii.crc32(data) & 0xffffffff)
    blob += data
    count += 1

out.parent.mkdir(parents=True, exist_ok=True)
with open(out, 'wb') as f:
    f.write(stub.read_bytes())
    f.write(blob)
    f.write(struct.pack('<I', len(blob)))
    f.write(b'RPSX')

print('Prepared RPSET16.EXE:', count, 'files,', out.stat().st_size, 'bytes')
