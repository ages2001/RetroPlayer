"""Compare the current package with the four cleanly stopped test disks.

VMIO commits only disposable overlays and refuses a running guest. Original
uploads are never modified. Generated plugin caches and guest logs are not
portable package inputs and are excluded from this comparison.
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
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
if a.output.resolve().parent != root/'RESULTS' or not re.fullmatch(
        '[A-Z0-9_]{1,8}\\.JSN', a.output.name):
    p.error('Output must be an 8.3 JSON report inside RESULTS')
base = root/'BUILD/ASM'
files = set(base.glob('*.DLL')) | set((base/'PLUGINS').glob('*.DLL'))
files |= set(base.glob('*.EXE'))
files.add(base/'DEJAVU.TTF')
manifests = {}
for name in ('AFREFS.JSN', 'VFREFS.JSN', 'AVREFS.JSN'):
    manifest = base/name
    manifests[name] = hashlib.sha256(manifest.read_bytes()).hexdigest()
    for item in json.loads(manifest.read_text()):
        for key, digest in [('file', 'sha256'), ('reference', 'reference_sha256')]:
            if key not in item:
                continue
            source = base/item[key]
            if hashlib.sha256(source.read_bytes()).hexdigest() != item[digest]:
                raise SystemExit('Fixture does not match manifest: '+str(source))
            files.add(source)
expected = {str(f.relative_to(base)): hashlib.sha256(f.read_bytes()).hexdigest()
            for f in sorted(files)}
report = {'scope': __doc__, 'package_sha256': expected,
          'fixture_manifests': manifests, 'guests': {}}
failures = 0
for guest, stem in [('NT31', 'NT31'), ('NT351', 'N351'), ('NT40', 'NT40'), ('W95', 'W95')]:
    subprocess.run([sys.executable, str(root/'SCRIPTS/VMIO.PY'), guest], check=True)
    disk = str(root/'TESTDISK'/(stem+'EDIT.IMG'))+'@@32256'
    folder = '::/RETROPLA/' if guest in ('NT31', 'NT351') else '::/Program Files/RetroPlayer/'
    compared = {}
    for name, digest in expected.items():
        result = subprocess.run(['mtype', '-i', disk, folder+name], capture_output=True)
        actual = hashlib.sha256(result.stdout).hexdigest() if result.returncode == 0 else None
        compared[name] = {'sha256': actual, 'result': 'PASS' if actual == digest else 'FAIL'}
        if actual != digest:
            failures += 1
            print(guest, name, 'MISSING' if actual is None else 'DIFFERENT', flush=True)
    report['guests'][guest] = compared
    print(guest, 'compared', len(compared), 'files', flush=True)
report['failures'] = failures
report['result'] = 'FAIL' if failures else 'PASS'
a.output.write_text(json.dumps(report, indent=2)+'\n')
raise SystemExit(bool(failures))
