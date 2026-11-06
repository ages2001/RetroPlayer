"""Map built VLC modules to stable 8.3 names, rejecting hash collisions."""
import argparse
import hashlib
from pathlib import Path
import shutil


def plugin_name(module):
    return 'P' + hashlib.sha256(module.encode('ascii')).hexdigest()[:7].upper() + '.DLL'


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('destination', type=Path)
    p.add_argument('modules', nargs='+')
    a = p.parse_args()
    mapping = {}
    for module in sorted(set(a.modules)):
        if not module or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_' for c in module):
            raise SystemExit('Invalid module name: ' + module)
        name = plugin_name(module)
        if name in mapping:
            raise SystemExit('8.3 hash collision: ' + module + ' / ' + mapping[name])
        source = a.source/('lib' + module + '_plugin.dll')
        if not source.is_file():
            raise SystemExit('Missing built module: ' + str(source))
        mapping[name] = module
    a.destination.mkdir(parents=True, exist_ok=True)
    for name, module in mapping.items():
        shutil.copyfile(a.source/('lib' + module + '_plugin.dll'), a.destination/name)
    (a.destination/'UPSTREAM.TXT').write_text(
        'VLC module filename mapping\n' + ''.join(f'{name} {module}\n' for name, module in mapping.items()),
        encoding='ascii')
