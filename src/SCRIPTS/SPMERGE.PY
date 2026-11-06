"""Integrate only the independently validated Ogg/Speex candidate modules.

This does not copy other modules/core from the experimental build directory,
rebuild the installer, or claim a complete Win32 release.
"""
import hashlib
import json
from pathlib import Path
import runpy
import shutil

root = Path(__file__).resolve().parents[1]
results = root/'RESULTS'
output = results/'SPINTEG.JSN'
if output.exists():
    raise SystemExit('Speex integration already recorded; preserve its evidence')
evidence = {}
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def record(path):
    evidence[str(path.relative_to(root))] = digest(path)
def require(condition, message):
    if not condition:
        raise SystemExit(message)

for prefix in ('N31', 'N351', 'N40', 'W95'):
    path = results/(prefix+'S4.JSN')
    suite = json.loads(path.read_text())
    require(suite['result'] == 'PASS', 'Speex suite failed: '+prefix)
    require(set(suite['programs']) == {'CACHE.EXE', 'SPRES.EXE', 'SPTEST.EXE', 'SROLD.EXE'},
            'Incomplete Speex suite: '+prefix)
    record(path)
    for program in suite['programs'].values():
        require(program['exit'] == 'EXIT 0' and program['result'] == 'PASS',
                'Failed Speex program: '+prefix)
        for name, expected in program['reports'].items():
            path = results/name
            require(digest(path) == expected, 'Speex evidence changed: '+name)
            record(path)
    path = results/(prefix+'S5.JSN')
    regression = json.loads(path.read_text())
    require(regression.get('AFMTEST.EXE') == 'EXIT 0', 'Audio regression missing: '+prefix)
    record(path)
    path = results/(prefix+'S5AF.TXT')
    require('FORMATS=24 FAILURES=0' in path.read_text(), 'Audio cases failed: '+prefix)
    record(path)
    tag = 'S6' if prefix == 'N31' else 'S5'
    path = results/(prefix+tag+'.JSN')
    require(json.loads(path.read_text()).get('VFMTEST.EXE') == 'EXIT 0',
            'Video regression missing: '+prefix)
    record(path)
    path = results/(prefix+tag+'VF.TXT')
    require('CHECKS=82 FAILURES=0' in path.read_text(), 'Video cases failed: '+prefix)
    record(path)

stage = json.loads((results/'N31S4ST.JSN').read_text())['files']
other_stage = json.loads((results/'S4STAGE.JSN').read_text())['files']
mapping_path = root/'BUILD/SPMOD/UPSTREAM.TXT'
mapping = dict(line.split() for line in mapping_path.read_text().splitlines()[1:])
require(set(mapping.values()) == {'ogg', 'speex', 'speex_resampler'}, 'Unexpected candidates')
for name in mapping:
    key = 'BUILD/SPSTAGE/PLUGINS/'+name
    require(digest(root/'BUILD/SPMOD'/name) == stage[key] == other_stage[key],
            'Candidate differs from guest-tested bytes: '+name)
    record(root/'BUILD/SPMOD'/name)
for name in ('N31S4ST.JSN', 'S4STAGE.JSN'):
    record(results/name)

target = root/'BUILD/ASM/PLUGINS'
old_mapping = target/'UPSTREAM.TXT'
all_modules = dict(line.split() for line in old_mapping.read_text().splitlines()[1:])
for name, module in mapping.items():
    require(name not in all_modules or all_modules[name] == module, 'Module filename collision')
all_modules.update(mapping)
require(len(set(all_modules.values())) == len(all_modules), 'Duplicate module mapping')
backup = root/'BUILD/SPBASE/PLUGINS'
require(not backup.exists(), 'Prior Speex backup exists; inspect before replacing')
backup.mkdir(parents=True)
shutil.copyfile(old_mapping, backup/'UPSTREAM.TXT')
for name in mapping:
    if (target/name).exists():
        shutil.copyfile(target/name, backup/name)
    shutil.copyfile(root/'BUILD/SPMOD'/name, target/name)
old_mapping.write_text('VLC module filename mapping\n'+''.join(
    name+' '+module+'\n' for name, module in sorted(all_modules.items(), key=lambda pair: pair[1])))
audit = runpy.run_path(str(root/'SCRIPTS/AUDIT.PY'))
audit['audit_names'](target)
for name in mapping:
    audit['pe_info'](target/name)
output.write_text(json.dumps({'scope': __doc__, 'result': 'PASS',
    'module_count': len(all_modules), 'modules': mapping, 'evidence': evidence,
    'installer_updated': False, 'full_win32_complete': False}, indent=2)+'\n')
print('Integrated', len(mapping), 'validated module files;', len(all_modules), 'modules in ASM')
