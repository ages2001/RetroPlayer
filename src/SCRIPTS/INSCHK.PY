"""Compare installed payload bytes after a successful GUI installation.

Only stopped disposable test disks are inspected. VMIO validates the backing
disk and commits the test overlay; original uploaded disks remain untouched.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('NT31', 'NT351', 'NT40', 'W95'))
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
if a.output.resolve().parent != root/'RESULTS' or not re.fullmatch(r'[A-Z0-9_]{1,8}\.JSN', a.output.name):
    p.error('Output must be an 8.3 JSON report inside RESULTS')
setup = root/'BUILD/SETUP'
expected = {}
for row in (setup/'FILES.SHA').read_text().splitlines():
    digest, relative = row.split('  ', 1)
    path = setup/relative
    if not path.is_relative_to(setup/'PAYLOAD') or '..' in path.parts:
        raise SystemExit('Invalid payload path')
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit('Host payload changed: '+relative)
    expected[str(path.relative_to(setup/'PAYLOAD'))] = digest
subprocess.run([sys.executable, str(root/'SCRIPTS/VMIO.PY'), a.guest], check=True)
stem = 'N351' if a.guest == 'NT351' else a.guest
disk = str(root/'TESTDISK'/(stem+'EDIT.IMG'))+'@@32256'
folder = '::/RETROPLA/' if a.guest in ('NT31', 'NT351') else '::/Program Files/RetroPlayer/'
compared = {}
for name, digest in expected.items():
    result = subprocess.run(['mtype', '-i', disk, folder+name], capture_output=True)
    actual = hashlib.sha256(result.stdout).hexdigest() if result.returncode == 0 else None
    compared[name] = {'expected': digest, 'actual': actual, 'result': 'PASS' if actual == digest else 'FAIL'}
failures = [name for name, result in compared.items() if result['result'] != 'PASS']
report = {'scope': __doc__, 'guest': a.guest, 'directory': folder,
          'manifest_sha256': hashlib.sha256((setup/'FILES.DAT').read_bytes()).hexdigest(),
          'files': compared, 'failures': failures, 'result': 'FAIL' if failures else 'PASS'}
a.output.write_text(json.dumps(report, indent=2)+'\n')
print(a.guest, report['result'], len(compared), 'payload files;', len(failures), 'mismatches')
raise SystemExit(bool(failures))
