"""Observe the Opus packet-size test interval in an emulated sound capture."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('guest',choices=('NT31','NT351','NT40','W95'))
p.add_argument('--tag',required=True)
p.add_argument('--device',choices=('hda','sb16'),default='hda')
a=p.parse_args()
if not re.fullmatch('[A-Z0-9]{2}',a.tag):p.error('Tag must contain two uppercase letters/digits')
prefix={'NT31':'N31','NT351':'N351','NT40':'N40','W95':'W95'}[a.guest]+a.tag
bounds=root/'RESULTS'/(prefix+'PB.JSN')
if bounds.exists():p.error('Choose a new tag; prior results are preserved')
capture=root/'RESULTS'/(a.guest+('SND.WAV' if a.device=='hda' else 'SB.WAV'))
def command(*args):
    return subprocess.check_output([sys.executable,str(root/'SCRIPTS/VMRUN.PY'),a.guest,*args],text=True).strip()
before=max(0,capture.stat().st_size-44)//4
if command('RUN','QPACK.EXE')!='STARTED':raise RuntimeError('Could not start QPACK')
print(a.guest,'QPACK STARTED',flush=True)
deadline=time.monotonic()+150
status='TIMEOUT'
while time.monotonic()<deadline:
    status=command('STAT')
    if status!='BUSY':break
    time.sleep(1)
record={'scope':__doc__,'guest':a.guest,'device':a.device,'capture':capture.name,
        'before':before,'after':max(0,capture.stat().st_size-44)//4,'guest_exit':status,
        'harness_sha256':hashlib.sha256((root/'BUILD/QPACK.EXE').read_bytes()).hexdigest()}
bounds.write_text(json.dumps(record,indent=2)+'\n')
for name,suffix in [('QPACK.TXT','PK'),('QPLOG.TXT','PL')]:
    command('GET',name,'--output',str(root/'RESULTS'/(prefix+suffix+'.TXT')))
print(a.guest,'QPACK',status,'; analyze the closed capture separately',flush=True)
raise SystemExit(status!='EXIT 0')
