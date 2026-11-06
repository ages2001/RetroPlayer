"""Build a private NT 3.51 test VHD from installed files and an NT FAT16 seed.

The seed supplies compatible MBR/PBR boot code. It is opened read-only and
converted to a temporary raw image; neither the seed nor the ZIP is modified.
No operating system files are part of the RetroPlayer distribution.
"""
import argparse
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import tempfile
import zipfile


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


def build(archive, seed, output):
    if output.exists():
        raise ValueError(f'Refusing to replace existing disk: {output}')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='nt351-', dir=output.parent) as tmp:
        work = Path(tmp)
        installed = work / 'FILES'
        installed.mkdir()
        with zipfile.ZipFile(archive) as source:
            entries, names = [], set()
            for entry in source.infolist():
                path = PurePosixPath(entry.filename.replace('\\', '/'))
                if (path.is_absolute() or not path.parts or
                        any(p in ('.', '..') or ':' in p for p in path.parts)):
                    raise ValueError(f'Unsafe archive path: {entry.filename}')
                if path.parts[0].upper() != 'NT351':
                    raise ValueError('Expected an NT351 root directory')
                if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError('Archive symlinks are not supported')
                rel = Path(*path.parts[1:])
                if not rel.parts:
                    continue
                key = str(rel).upper()
                if key in names:
                    raise ValueError(f'Duplicate archive path: {rel}')
                names.add(key)
                entries.append((entry, rel))
            if sum(e.file_size for e, _ in entries) > 300 * 1024 * 1024:
                raise ValueError('Installed files exceed the test disk budget')
            for entry, rel in entries:
                dest = installed / rel
                if entry.is_dir():
                    dest.mkdir(parents=True, exist_ok=True)
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    with source.open(entry) as src, dest.open('xb') as out:
                        shutil.copyfileobj(src, out)
        # Case-insensitive lookup also checks that this is an installed system.
        def find(name):
            matches = [p for p in installed.rglob('*')
                       if str(p.relative_to(installed)).upper() == name.upper()]
            if len(matches) != 1:
                raise ValueError(f'Missing or ambiguous installed file: {name}')
            return matches[0]
        for name in ('NTLDR', 'NTDETECT.COM', 'WINNT351/SYSTEM32/NTOSKRNL.EXE',
                     'WINNT351/SYSTEM32/HAL.DLL',
                     'WINNT351/SYSTEM32/CONFIG/SYSTEM'):
            find(name)
        raw_seed = work / 'SEED.IMG'
        run('qemu-img', 'convert', '-f', 'vpc', '-O', 'raw', seed, raw_seed)
        with raw_seed.open('rb') as src:
            mbr = src.read(512)
            part = mbr[446:462]
            start, sectors = struct.unpack_from('<II', part, 8)
            if mbr[510:] != b'\x55\xaa' or part[0] != 0x80 or part[4] != 6:
                raise ValueError('Seed must have an active first FAT16 partition')
            if not start or start + sectors > raw_seed.stat().st_size // 512:
                raise ValueError('Seed partition lies outside the disk')
            if any(mbr[462:510]):
                raise ValueError('Only a single-partition seed is supported')
            src.seek(start * 512)
            pbr = src.read(512)
        if pbr[510:] != b'\x55\xaa' or b'NTLDR' not in pbr:
            raise ValueError('Seed has no recognized NT FAT boot sector')
        bps = struct.unpack_from('<H', pbr, 11)[0]
        spt, heads = struct.unpack_from('<HH', pbr, 24)
        if bps != 512 or not spt or not heads:
            raise ValueError('Unsupported seed geometry')
        boot = work / 'BOOT.BIN'
        boot.write_bytes(pbr)
        raw = work / 'NT351.IMG'
        with raw.open('xb') as disk:
            disk.write(mbr)
            disk.truncate(raw_seed.stat().st_size)
        volume = f'{raw}@@{start * 512}'
        run('mformat', '-i', volume, '-B', boot, '-H', start,
            '-h', heads, '-s', spt, '-T', sectors, '::')
        # NTLDR must be placed before the larger installed tree on FAT.
        run('mcopy', '-i', volume, find('NTLDR'), '::NTLDR')
        run('mcopy', '-i', volume, find('NTDETECT.COM'), '::NTDETECT.COM')
        ini = work / 'BOOT.INI'
        arc = r'multi(0)disk(0)rdisk(0)partition(1)\WINNT351'
        ini.write_bytes(('[boot loader]\r\ntimeout=3\r\ndefault=' + arc +
            '\r\n[operating systems]\r\n' + arc +
            '="Windows NT Workstation 3.51"\r\n').encode('ascii'))
        run('mcopy', '-i', volume, ini, '::BOOT.INI')
        run('mcopy', '-s', '-i', volume, find('WINNT351'), '::')
        result = work / 'NT351.VHD'
        run('qemu-img', 'convert', '-f', 'raw', '-O', 'vpc',
            '-o', 'subformat=dynamic', raw, result)
        # A hard link publishes only a finished disk and cannot overwrite a
        # file created by another process after the initial existence check.
        os.link(result, output)
    print(f'Created {output}; boot verification is still required.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('seed', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.archive.resolve(), args.seed.resolve(), args.output.resolve())
