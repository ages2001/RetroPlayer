"""Test production drift control against deterministic timing and packet sizes.

The device/clock boundary is simulated. The controller itself is extracted
from the production C file, not reimplemented in the test. Guest captures
remain necessary to establish output behavior.
"""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[1]
out = root/'BUILD/SYNCTEST'
out.mkdir(exist_ok=True)
report = root/'RESULTS/SYNCUNIT.JSN'
if report.exists():
    raise SystemExit('Preserve the previous controller test report')
sources = [('current', Path('/workspace/VLCWORK/src/audio_output/dec.c')),
           ('baseline', root/'SOURCES/AOUTTG.C')]
results = []
for label, source in sources:
    text = source.read_text()
    functions = []
    for name in ('aout_StopResampling', 'aout_DecSynchronize'):
        start = text.index('static void '+name+' ')
        end = text.index('\n}\n', start)+3
        functions.append(text[start:end])
    include = out/'SYNCFUN.H'
    include.write_text('\n'.join(functions))
    target = out/('CURRENT' if label == 'current' else 'BASELINE')
    subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    *(['-DRP_BASELINE'] if label == 'baseline' else []),
                    '-x', 'c', '-I'+str(out), str(root/'TESTS/SYNCRATE.C'),
                    '-o', str(target)], check=True)
    result = subprocess.run([str(target)], capture_output=True, text=True)
    results.append({'source': label, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'controller_sha256': hashlib.sha256(include.read_bytes()).hexdigest(),
                    'exit_code': result.returncode, 'output': result.stdout})
    print(label, result.stdout.rstrip().splitlines()[-1])
passed = results[0]['exit_code'] == 0 and results[1]['exit_code'] != 0
report.write_text(json.dumps({'scope': __doc__, 'results': results,
                             'result': 'PASS' if passed else 'FAIL'}, indent=2)+'\n')
raise SystemExit(not passed)
