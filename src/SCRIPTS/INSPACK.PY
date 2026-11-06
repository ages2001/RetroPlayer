"""Append the 32-bit player payload to the self-extracting installer stub.

Produces BUILD/SETUP/RPSET32.EXE = INSTALL.EXE stub + payload + trailer.
Payload entry: <name_len u16 LE><name><data_len u32 LE><crc32 u32 LE><data>.
Trailer: <payload_size u32 LE>"RPSX". Names are DOS 8.3 relative paths,
uppercased, backslash-separated. No OS files, drivers or registry entries are
packaged; CRC32 catches a damaged download, it is not an authenticity check.
"""
import binascii
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
stub = root / 'BUILD/SETUP/INSTALL.EXE'
source = root / 'BIN'                      # the ready-to-run 32-bit player
out = root / 'BUILD/SETUP/RPSET32.EXE'

NAME_OK = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-')


def check_83(name):
    for part in name.split('\\'):
        base, dot, ext = part.partition('.')
        if not base or len(base) > 8 or len(ext) > 3:
            raise SystemExit('Not an 8.3 name: ' + name)
        if any(c not in NAME_OK for c in base + ext):
            raise SystemExit('Illegal character in name: ' + name)


entries = []


def add(relname, path):
    name = relname.replace('/', '\\').upper()
    check_83(name)
    entries.append((name, path.read_bytes()))


for p in sorted(source.glob('*')):
    if p.is_file():
        add(p.name, p)
for subdir in ('PLUGINS', 'LICENSES'):
    d = source / subdir
    if d.is_dir():
        for p in sorted(d.glob('*')):
            if p.is_file():
                add(subdir + '\\' + p.name, p)

if len(entries) > 512:
    raise SystemExit('Too many payload files for the installer (max 512)')

blob = bytearray()
for name, data in entries:
    nb = name.encode('ascii')
    blob += struct.pack('<H', len(nb)) + nb
    blob += struct.pack('<II', len(data), binascii.crc32(data) & 0xffffffff)
    blob += data

with open(out, 'wb') as f:
    f.write(stub.read_bytes())
    f.write(blob)
    f.write(struct.pack('<I', len(blob)))
    f.write(b'RPSX')

print('Prepared RPSET32.EXE:', len(entries), 'files,',
      out.stat().st_size, 'bytes (payload', len(blob), 'bytes)')
