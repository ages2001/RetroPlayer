"""Run native Speex regressions on an already staged private guest."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('NT31', 'NT351', 'NT40', 'W95'))
p.add_argument('--tag', required=True)
p.add_argument('--baseline', action='store_true')
a = p.parse_args()
if not re.fullmatch('[A-Z0-9]{2}', a.tag):
    p.error('Tag must contain two uppercase letters/digits')
prefix = {'NT31':'N31', 'NT351':'N351', 'NT40':'N40', 'W95':'W95'}[a.guest]+a.tag
output = root/'RESULTS'/(prefix+'.JSN')
if output.exists():
    p.error('Choose a new tag; prior results are preserved')
def command(*args):
    return subprocess.check_output([sys.executable, str(root/'SCRIPTS/VMRUN.PY'), a.guest, *args], text=True).strip()
jobs = [('CACHE.EXE', 'EXIT 0', [('CACHE.TXT', 'CA')]),
        ('SPRES.EXE', 'EXIT 0', [('SPRES.TXT', 'SR')]),
        ('SPTEST.EXE', 'EXIT 0', [('SPTEST.TXT', 'SP'), ('SPLOG.TXT', 'SL')])]
if a.baseline:
    jobs.append(('SROLD.EXE', 'EXIT 0', [('SROLD.TXT', 'SO')]))
record = {'scope': __doc__, 'guest': a.guest, 'programs': {}, 'result': 'INCOMPLETE'}
for program, expected, reports in jobs:
    if command('RUN', program) != 'STARTED':
        raise RuntimeError('Could not start '+program)
    print(a.guest, program, 'STARTED', flush=True)
    deadline = time.monotonic()+900
    while time.monotonic() < deadline:
        status = command('STAT')
        if status != 'BUSY':
            break
        time.sleep(1)
    else:
        raise TimeoutError(program+' timed out')
    entry = {'exit': status, 'expected_exit': expected, 'reports': {}, 'result': 'PASS' if status == expected else 'FAIL'}
    record['programs'][program] = entry
    output.write_text(json.dumps(record, indent=2)+'\n')
    for name, suffix in reports:
        path = root/'RESULTS'/(prefix+suffix+'.TXT')
        try:
            command('GET', name, '--output', str(path))
            data = path.read_bytes()
            entry['reports'][path.name] = hashlib.sha256(data).hexdigest()
            if name != 'SPLOG.TXT':
                marker = b'RESULT=FAIL' if expected == 'EXIT 1' else b'RESULT=PASS'
                if marker not in data:
                    entry['result'] = 'FAIL'
        except subprocess.CalledProcessError as error:
            entry['reports'][path.name] = {'error': error.output}
            entry['result'] = 'FAIL'
    output.write_text(json.dumps(record, indent=2)+'\n')
    print(a.guest, program, status, entry['result'], flush=True)
record['result'] = 'PASS' if all(item['result'] == 'PASS' for item in record['programs'].values()) else 'FAIL'
output.write_text(json.dumps(record, indent=2)+'\n')
raise SystemExit(record['result'] != 'PASS')
