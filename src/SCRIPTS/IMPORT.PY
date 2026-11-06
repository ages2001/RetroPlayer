"""Compare PE imports with actual guest exports without executing guest code.

This diagnostic handles named/ordinal exports and export forwarders. Export
presence does not prove behavior: Windows 95 has unimplemented Unicode APIs.
Only application modules are traversed; the supplied OS is assumed internally
consistent. Delayed imports are reported separately, not treated as load-time
requirements. Requires the pinned pefile package from SETUP.SH.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import pefile


def index(directories):
    result = {}
    for directory in directories:
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix.lower() in ('.dll', '.exe'):
                name = path.name.lower()
                if name in result and result[name].resolve() != path.resolve():
                    raise ValueError(f'Ambiguous module: {name}')
                result[name] = path
    return result


def inspect(roots, local, system):
    cache = {}
    exports = {}
    def read(path):
        if path not in cache:
            pe = pefile.PE(str(path), fast_load=True)
            pe.parse_data_directories(directories=[
                pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT'],
                pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT'],
                pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT']])
            cache[path] = pe
            table = getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None)
            lookup = {}
            for entry in table.symbols if table else []:
                lookup[entry.ordinal] = entry
                if entry.name: lookup[entry.name.decode('ascii')] = entry
            exports[path] = lookup
        return cache[path]
    def locate(name):
        return local.get(name.lower()) or system.get(name.lower())
    def resolve(dll, symbol, seen=frozenset()):
        key = (dll.lower(), symbol)
        if key in seen:
            return 'forwarder cycle'
        path = locate(dll)
        if not path:
            return 'missing DLL'
        try:
            pe = read(path)
        except pefile.PEFormatError:
            return 'not a PE image'
        export = exports[path].get(symbol)
        if export:
            if not export.forwarder:
                return None if export.address else 'null export address'
            dest, target = export.forwarder.decode('ascii').rsplit('.', 1)
            if not dest.lower().endswith('.dll'): dest += '.dll'
            if target.startswith('#'): target = int(target[1:])
            error = resolve(dest, target, seen | {key})
            return f'{export.forwarder.decode()}: {error}' if error else None
        return 'missing export'
    reports, queue, visited = [], list(roots), set()
    while queue:
        path = queue.pop(0).resolve()
        if path in visited: continue
        visited.add(path)
        pe = read(path)
        report = {'file': path.name, 'machine': pe.FILE_HEADER.Machine,
            'subsystem': [pe.OPTIONAL_HEADER.MajorSubsystemVersion,
                          pe.OPTIONAL_HEADER.MinorSubsystemVersion],
            'name_8_3': bool(re.fullmatch(r'[A-Za-z0-9_]{1,8}\.[A-Za-z0-9_]{1,3}', path.name)),
            'missing': {}, 'delayed': {}, 'imports': 0}
        for table, output in [('DIRECTORY_ENTRY_IMPORT', 'missing'),
                              ('DIRECTORY_ENTRY_DELAY_IMPORT', 'delayed')]:
            for descriptor in getattr(pe, table, []):
                dll = descriptor.dll.decode('ascii')
                if table == 'DIRECTORY_ENTRY_IMPORT' and dll.lower() in local:
                    queue.append(local[dll.lower()])
                problems = []
                for entry in descriptor.imports:
                    symbol = entry.name.decode('ascii') if entry.name else entry.ordinal
                    error = resolve(dll, symbol)
                    if error:
                        problems.append({'symbol': symbol, 'reason': error})
                    if table == 'DIRECTORY_ENTRY_IMPORT': report['imports'] += 1
                if problems: report[output][dll] = problems
        reports.append(report)
    for pe in cache.values(): pe.close()
    return reports


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--local', type=Path, action='append', default=[])
    parser.add_argument('--system', type=Path, action='append', required=True)
    parser.add_argument('modules', type=Path, nargs='+')
    args = parser.parse_args()
    reports = inspect(args.modules, index(args.local), index(args.system))
    print(json.dumps({'scope': 'Static imports; not a playback or runtime test',
                      'modules': reports}, indent=2))
    sys.exit(1 if any(r['missing'] for r in reports) else 0)
