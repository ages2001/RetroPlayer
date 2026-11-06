"""Transfer private test files only while the disposable guest disk is offline.

The guest must already have shut down cleanly and QEMU must have exited.
Original uploaded disks are never writable targets of this utility.
"""
import argparse
import json
import re
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('NT31','NT351','NT40','W95','W2K','XP64'))
p.add_argument('--put', type=Path, action='append', default=[])
p.add_argument('--get', nargs=2, metavar=('REPORT','OUTPUT'), action='append', default=[])
a = p.parse_args()
root = Path(__file__).resolve().parent.parent
stem = 'N351' if a.guest=='NT351' else a.guest
overlay = root/'TESTDISK'/(stem+'EDIT.QC2')
raw = root/'TESTDISK'/(stem+'EDIT.IMG')
for entry in Path('/proc').iterdir():
    if not entry.name.isdigit(): continue
    try: command = (entry/'cmdline').read_bytes().split(b'\0')
    except OSError: continue
    if command and b'qemu-system' in command[0] and any(
            b'file='+str(overlay).encode()+b',' in item for item in command):
        raise SystemExit('Guest is running; no files were transferred')
info = json.loads(subprocess.check_output(['qemu-img','info','--output=json',str(overlay)]))
if Path(info.get('full-backing-filename','')).resolve()!=raw.resolve():
    raise SystemExit('Refusing an unexpected backing disk')
if info.get('backing-filename-format')!='raw':
    raise SystemExit('Only disposable raw backing disks may be modified')
def name_ok(name):
    return bool(re.fullmatch(r'[A-Za-z0-9_]{1,8}\.[A-Za-z0-9_]{1,3}',name))
for report,output in a.get:
    if not name_ok(report) or not name_ok(output):
        raise SystemExit('Reports must use DOS 8.3 names')
for source in a.put:
    if not source.is_file() or not name_ok(source.name):
        raise SystemExit('Invalid application file: '+str(source))
subprocess.run(['qemu-img','commit',str(overlay)],check=True)
disk = str(raw)+'@@32256'
folder = '::/RETROPLA/' if a.guest in ('NT31','NT351') else '::/Program Files/RetroPlayer/'
for report,output in a.get:
    subprocess.run(['mcopy','-o','-i',disk,folder+report,str(root/'RESULTS'/output)],check=True)
for source in a.put:
    destination = folder+('PLUGINS/' if source.parent.name.upper()=='PLUGINS' else '')
    subprocess.run(['mcopy','-o','-i',disk,str(source),destination],check=True)
