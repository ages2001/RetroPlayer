"""Generate the RetroPlayer icons.

Light "media player" look: a glossy blue play button on a light disc. Each .ico
holds true-colour (32bpp) images for modern displays and 16-colour (4bpp DIB)
images for Windows 3.x / NT 3.x / 95 Program Manager, at 48/32/16 px. The
uninstaller icon is the application icon with a bold red X over it.

Outputs:
  SRC/RETRO.ICO      - 32-bit player application icon
  SRC/RPUNIN.ICO     - uninstaller icon (app icon + red X)
  SRC16/RETRO16.ICO  - 16-bit player application icon (same art)

Requires Pillow (build-time asset generation only; not shipped or run on guest).
"""
import struct
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SS = 8  # supersample factor for the true-colour art

# Windows standard 16-colour palette (indices used below).
PAL16 = [(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128),(0,128,128),
         (192,192,192),(128,128,128),(255,0,0),(0,255,0),(255,255,0),(0,0,255),
         (255,0,255),(0,255,255),(255,255,255)]
WHITE,SILVER,GRAY,NAVY,BLUE,RED = 15,7,8,4,12,9


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i]-a[i])*t) for i in range(len(a)))


def render_truecolor(size, uninstall):
    S = size*SS
    img = Image.new('RGBA', (S, S), (0,0,0,0))
    m = int(S*0.06)
    top, bot = (252,253,255), (176,203,238)
    body = Image.new('RGBA', (S, S), (0,0,0,0)); bd = ImageDraw.Draw(body)
    for y in range(m, S-m):
        bd.line([(m,y),(S-m,y)], fill=lerp(top, bot, (y-m)/(S-2*m))+(255,))
    mask = Image.new('L', (S, S), 0); ImageDraw.Draw(mask).ellipse([m,m,S-m,S-m], fill=255)
    img.paste(body, (0,0), mask)
    gloss = Image.new('RGBA', (S, S), (0,0,0,0))
    ImageDraw.Draw(gloss).ellipse([m+int(S*0.10), m+int(S*0.05), S-m-int(S*0.10), int(S*0.48)],
                                  fill=(255,255,255,120))
    img.alpha_composite(gloss)
    d = ImageDraw.Draw(img)
    d.ellipse([m,m,S-m,S-m], outline=(60,110,185,255), width=max(2,int(S*0.035)))
    cx, cy, r = S*0.52, S*0.5, S*0.24
    tri = [(cx-r*0.8,cy-r),(cx-r*0.8,cy+r),(cx+r,cy)]
    tmask = Image.new('L', (S, S), 0); ImageDraw.Draw(tmask).polygon(tri, fill=255)
    tg = Image.new('RGBA', (S, S), (0,0,0,0)); tgd = ImageDraw.Draw(tg)
    ys = [p[1] for p in tri]; y0, y1 = min(ys), max(ys)
    for y in range(int(y0), int(y1)+1):
        tgd.line([(0,y),(S,y)], fill=lerp((74,140,224),(24,74,168),(y-y0)/max(1,y1-y0))+(255,))
    img.paste(tg, (0,0), tmask)
    if uninstall:
        ov = Image.new('RGBA', (S, S), (0,0,0,0)); od = ImageDraw.Draw(ov)
        w, a, b = int(S*0.14), int(S*0.14), S-int(S*0.14)
        od.line([(a,a),(b,b)], fill=(206,0,0,255), width=w)
        od.line([(a,b),(b,a)], fill=(206,0,0,255), width=w)
        img.alpha_composite(ov)
    return img.resize((size, size), Image.LANCZOS)


def render_flat16(size, uninstall):
    """Palette-exact art for the 4bpp (16-colour) image: hard edges, no gradient."""
    SS2 = 8; S = size*SS2
    img = Image.new('RGBA', (S, S), (0,0,0,0)); d = ImageDraw.Draw(img)
    c = lambda i: PAL16[i]+(255,)
    m = int(S*0.06)
    d.ellipse([m,m,S-m,S-m], fill=c(SILVER))
    d.ellipse([m+int(S*0.12),m+int(S*0.08),S-m-int(S*0.30),int(S*0.46)], fill=c(WHITE))
    d.ellipse([m,m,S-m,S-m], outline=c(NAVY), width=max(2,int(S*0.05)))
    cx, cy, r = S*0.53, S*0.5, S*0.24
    d.polygon([(cx-r*0.8,cy-r),(cx-r*0.8,cy+r),(cx+r,cy)], fill=c(BLUE), outline=c(NAVY))
    if uninstall:
        w, a, b = int(S*0.15), int(S*0.13), S-int(S*0.13)
        d.line([(a,a),(b,b)], fill=c(RED), width=w)
        d.line([(a,b),(b,a)], fill=c(RED), width=w)
    return img.resize((size, size), Image.NEAREST)


def and_mask_rows(rgba, size):
    """1bpp AND mask, bottom-up, rows padded to 4 bytes. 1 = transparent."""
    px = rgba.load(); rb = ((size+31)//32)*4; rows = []
    for y in range(size-1, -1, -1):
        row = bytearray(rb)
        for x in range(size):
            if px[x, y][3] < 128:
                row[x//8] |= 0x80 >> (x % 8)
        rows.append(bytes(row))
    return b''.join(rows)


def encode_32bpp(rgba, size):
    px = rgba.load(); xor = bytearray()
    for y in range(size-1, -1, -1):
        for x in range(size):
            r, g, b, a = px[x, y]
            xor += bytes((b, g, r, a))
    andm = and_mask_rows(rgba, size)
    bih = struct.pack('<IiiHHIIiiII', 40, size, size*2, 1, 32, 0, len(xor)+len(andm), 0,0,0,0)
    return bih + bytes(xor) + andm


def nearest_index(rgb):
    best, bi = 1<<30, 0
    for i, p in enumerate(PAL16):
        dd = sum((rgb[k]-p[k])**2 for k in range(3))
        if dd < best:
            best, bi = dd, i
    return bi


def encode_4bpp(rgba, size):
    px = rgba.load(); rb = ((size*4+31)//32)*4; xor = bytearray()
    for y in range(size-1, -1, -1):
        row = bytearray(rb)
        for x in range(size):
            r, g, b, a = px[x, y]
            v = 0 if a < 128 else nearest_index((r, g, b))
            if x % 2 == 0: row[x//2] |= v << 4
            else:          row[x//2] |= v
        xor += row
    andm = and_mask_rows(rgba, size)
    pal = b''.join(struct.pack('<BBBB', p[2], p[1], p[0], 0) for p in PAL16)
    bih = struct.pack('<IiiHHIIiiII', 40, size, size*2, 1, 4, 0, len(xor)+len(andm), 0,0,16,0)
    return bih + pal + bytes(xor) + andm


def build_ico(path, uninstall, sizes_true=(48,32,16), sizes_4=(32,16)):
    images = []  # (width, bitcount, data)
    for sz in sizes_true:
        images.append((sz, 32, encode_32bpp(render_truecolor(sz, uninstall), sz)))
    for sz in sizes_4:
        images.append((sz, 4, encode_4bpp(render_flat16(sz, uninstall), sz)))
    hdr = struct.pack('<HHH', 0, 1, len(images))
    entries = b''; offset = 6 + 16*len(images)
    for w, bits, data in images:
        colors = 16 if bits == 4 else 0
        ww = w if w < 256 else 0
        entries += struct.pack('<BBBBHHII', ww, ww, colors, 0, 1, bits, len(data), offset)
        offset += len(data)
    path.write_bytes(hdr + entries + b''.join(d for _,_,d in images))
    print('wrote', path, path.stat().st_size, 'bytes,', len(images), 'images')


build_ico(ROOT/'SRC/RETRO.ICO', uninstall=False)
build_ico(ROOT/'SRC/RPUNIN.ICO', uninstall=True)
build_ico(ROOT/'SRC16/RETRO16.ICO', uninstall=False)
