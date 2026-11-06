"""Fail closed on non-8.3 names, collisions and incompatible PE headers.

This is a packaging/import report, NOT proof of OS or CPU compatibility.
"""
import argparse
import json
import re
import struct
from pathlib import Path

SHORT = re.compile(r'[A-Za-z0-9_-]{1,8}(\.[A-Za-z0-9_-]{1,3})?\Z')
RESERVED = {'CON', 'PRN', 'AUX', 'NUL'} | {f'{p}{n}' for p in ('COM', 'LPT') for n in range(1, 10)}


def audit_names(root):
    seen = set()
    for f in sorted(root.rglob('*')):
        rel = f.relative_to(root)
        if f.is_symlink():
            raise ValueError(f'Symlinks are not supported in a DOS package: {rel}')
        key = str(rel).upper()
        if key in seen:
            raise ValueError(f'Case-insensitive name collision: {rel}')
        seen.add(key)
        if not SHORT.fullmatch(f.name) or f.name.split('.')[0].upper() in RESERVED:
            raise ValueError(f'Not a safe DOS 8.3 name: {rel}')


def pe_info(path):
    data = path.read_bytes()
    def u16(off): return struct.unpack_from('<H', data, off)[0]
    def u32(off): return struct.unpack_from('<I', data, off)[0]
    if data[:2] != b'MZ': raise ValueError(f'{path}: missing MZ')
    pe = u32(0x3c)
    if data[pe:pe+4] != b'PE\0\0': raise ValueError(f'{path}: not PE')
    if u16(pe+4) != 0x14c: raise ValueError(f'{path}: not x86')
    opt = pe+24
    if u16(opt) != 0x10b: raise ValueError(f'{path}: not PE32')
    subver = (u16(opt+48), u16(opt+50))
    if subver > (3, 10): raise ValueError(f'{path}: subsystem too new: {subver}')
    sections = []
    start = opt+u16(pe+20)
    for i in range(u16(pe+6)):
        s = start+i*40
        sections.append((u32(s+12), max(u32(s+8), u32(s+16)), u32(s+20)))
    def offset(rva):
        for va, size, raw in sections:
            if va <= rva < va+size: return raw+rva-va
        if rva < u32(opt+60): return rva
        raise ValueError(f'{path}: unmapped RVA {rva:x}')
    def cstr(off):
        end = data.index(b'\0', off)
        return data[off:end].decode('ascii')
    imports = {}
    rva = u32(opt+104)
    if rva:
        desc = offset(rva)
        while any(data[desc:desc+20]):
            dll = cstr(offset(u32(desc+12)))
            if not SHORT.fullmatch(dll):
                raise ValueError(f'{path}: imported DLL is not 8.3: {dll}')
            thunk = offset(u32(desc) or u32(desc+16))
            names = []
            while u32(thunk):
                val = u32(thunk)
                names.append(f'#{val & 0xffff}' if val & 0x80000000 else cstr(offset(val)+2))
                thunk += 4
            imports[dll] = names
            desc += 20
    return {'file': path.name, 'machine': 'i386', 'subsystem_version': subver,
            'imports': imports}


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('directory', type=Path)
    args = p.parse_args()
    if not args.directory.is_dir(): p.error('Directory does not exist')
    audit_names(args.directory)
    reports = [pe_info(f) for f in sorted(args.directory.rglob('*'))
               if f.suffix.upper() in ('.EXE', '.DLL')]
    if not reports: p.error('No executable files to validate')
    print(json.dumps(reports, indent=2))
