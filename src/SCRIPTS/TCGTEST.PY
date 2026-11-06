"""Exercise actual guarded calibration and clock conversion with synthetic APIs.

Checks timer quantization, timer wrap, stalls, discontinuities, missing TSC,
missing multimedia API and long-running arithmetic. Native guest tests remain
required; these simulated boundaries do not calibrate physical hardware.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import re
import sys

root=Path(__file__).resolve().parents[1]
source=Path(sys.argv[2]) if len(sys.argv)>2 else Path('/workspace/VLCWORK/src/win32/RPCLOCK.H')
tag=sys.argv[1] if len(sys.argv)>1 else 'TCGUNIT'
if not re.fullmatch(r'[A-Z0-9]{1,8}',tag): raise SystemExit('Invalid report tag')
report=root/'RESULTS'/(tag+'.JSN')
if report.exists(): raise SystemExit('Preserve the previous calibration test')
out=root/'BUILD/TCGTEST';out.mkdir(exist_ok=True)
text=source.read_text();functions=[]
for signature in ('static bool rp_tsc_setup(void)', 'static vlc_tick_t mdate_tsc(void)'):
    start=text.index(signature);end=text.index('\n}\n',start)+3
    functions.append(text[start:end])
include=out/'TCGFUN.H';include.write_text('\n'.join(functions))
subprocess.run(['cc','-x','c','-std=c99','-O2','-Wall','-Wextra','-Werror',
                '-I'+str(out),str(root/'TESTS/TCGCLOCK.C'),'-o',str(out/'TCGTEST')],check=True)
result=subprocess.run([str(out/'TCGTEST')],capture_output=True,text=True)
report.write_text(json.dumps({'scope':__doc__,
    'test_sha256':hashlib.sha256((root/'TESTS/TCGCLOCK.C').read_bytes()).hexdigest(),
    'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'functions_sha256':hashlib.sha256(include.read_bytes()).hexdigest(),
    'output':result.stdout,'exit_code':result.returncode,
    'result':'PASS' if result.returncode==0 else 'FAIL'},indent=2)+'\n')
print(result.stdout,end='');raise SystemExit(result.returncode)
