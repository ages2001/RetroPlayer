"""Compare streaming resampling with the previous stateless implementation."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('guest',choices=('ALL','NT31','NT351','NT40','W95'))
p.add_argument('--tag',required=True)
a=p.parse_args()
if not re.fullmatch('[A-Z0-9]{2}',a.tag):p.error('Tag must have two uppercase letters/digits')
prefixes={'NT31':'N31','NT351':'N351','NT40':'N40','W95':'W95'}
guests=list(prefixes) if a.guest=='ALL' else [a.guest]
for g in guests:
    prefix=prefixes[g]+a.tag
    if any((root/'RESULTS'/(prefix+tail)).exists() for tail in ['.JSN','RS.TXT','RB.TXT','RC.JSN']):
        p.error('Choose a new tag; prior reports are preserved: '+prefix)

def run(guest):
    prefix=prefixes[guest]+a.tag
    subprocess.run([sys.executable,str(root/'SCRIPTS/VMTEST.PY'),guest,
                    '--tag',a.tag,'--only','resampler'],check=True)
    def command(*args):
        return subprocess.check_output([sys.executable,str(root/'SCRIPTS/VMRUN.PY'),guest,*args],text=True).strip()
    if command('RUN','RESBASE.EXE')!='STARTED':raise RuntimeError('Negative control did not start')
    deadline=time.monotonic()+180
    while time.monotonic()<deadline:
        status=command('STAT')
        if status!='BUSY':break
        time.sleep(1)
    else:raise TimeoutError('Negative control timed out')
    old=root/'RESULTS'/(prefix+'RB.TXT')
    command('GET','RESBASE.TXT','--output',str(old))
    new=root/'RESULTS'/(prefix+'RS.TXT')
    line='STREAM IN=48002 OUT=48000 BLOCKS=2000 CHUNK=120 INPUT=240000 OUTPUT='
    passed=(status=='EXIT 1' and 'RESULT=FAIL' in old.read_text() and
            line+'238000 EXPECTED=239991' in old.read_text() and
            'RESULT=PASS' in new.read_text() and
            line+'239991 EXPECTED=239991' in new.read_text())
    result={'scope':'Actual resampler source, real core block allocator, arbitrary packets, flush and output-rate change.',
            'guest':guest,'negative_control_exit':status,
            'reports':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in [new,old]},
            'executables':{f:hashlib.sha256((root/'BUILD/RSTREAM'/f).read_bytes()).hexdigest()
                           for f in ['RESAMP.EXE','RESBASE.EXE']},
            'result':'PASS' if passed else 'FAIL'}
    (root/'RESULTS'/(prefix+'RC.JSN')).write_text(json.dumps(result,indent=2)+'\n')
    print(guest,'streaming comparison',result['result'],flush=True)
    return passed

with concurrent.futures.ThreadPoolExecutor(len(guests)) as pool:
    passed=list(pool.map(run,guests))
raise SystemExit(not all(passed))
