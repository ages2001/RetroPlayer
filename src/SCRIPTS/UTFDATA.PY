"""Create deterministic UTF-8 reference vectors using Python's codec."""
from pathlib import Path
import random
import struct

rng = random.Random(314159)
samples = [bytes([n]) for n in range(256)]
samples += [bytes(rng.randrange(256) for _ in range(rng.randrange(1, 9)))
            for _ in range(192)]
points = [0, 0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff,
          0x10000, 0x10ffff, 0x1f3a5]
points += [rng.randrange(0x110000) for _ in range(128)]
samples += [chr(p).encode('utf-8') for p in points if not 0xd800 <= p <= 0xdfff]
samples += [b'\xf0\x90\x80', b'\xe0\x80\x80', b'\xed\xa0\x80',
            b'\xf4\x90\x80\x80', b'\xc0\x80', b'A\0B', b'\xe2\x82X']
rows = []
for raw in samples:
    decoded = raw.decode('utf-8', errors='replace')
    try:
        raw.decode('utf-8', errors='strict')
        valid = 1
    except UnicodeDecodeError:
        valid = 0
    wide = struct.unpack('<' + 'H' * (len(decoded.encode('utf-16le')) // 2),
                         decoded.encode('utf-16le'))
    canonical = decoded.encode('utf-8')
    def array(seq): return '{' + ','.join(str(x) for x in seq) + '}'
    rows.append('{' + ','.join([str(len(raw)), str(len(wide)), str(len(canonical)),
        str(valid), array(raw), array(wide), array(canonical)]) + '}')
out = Path(__file__).resolve().parent.parent / 'BUILD' / 'UTFDATA.H'
out.parent.mkdir(exist_ok=True)
out.write_text('/* Generated codec reference data; do not edit. */\n'
    'typedef struct { int bytes, units, encoded, valid; unsigned char input[16]; '
    'WCHAR wide[16]; unsigned char canonical[48]; } UTFCASE;\n'
    'static const UTFCASE utf_cases[] = {\n' + ',\n'.join(rows) + '\n};\n')
print(f'Generated {len(rows)} independent codec reference vectors')
