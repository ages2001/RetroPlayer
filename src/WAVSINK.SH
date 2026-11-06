#!/bin/sh
# Build/test the host-only unbuffered observation helper; never distributed.
set -eu
cd "$(dirname "$0")"
mkdir -p BUILD/WAVSINK
python3 - <<'PY'
from pathlib import Path
helper=str(Path('BUILD/WAVSINK/WAVSINK.SO').resolve())
for proc in Path('/proc').iterdir():
    if not proc.name.isdigit(): continue
    try: maps=(proc/'maps').read_text()
    except (FileNotFoundError,ProcessLookupError,PermissionError): continue
    if helper in maps:
        raise SystemExit('The WAV helper is mapped by a live process; stop that VM before rebuilding')
PY
cc -x c -std=c99 -O2 -Wall -Wextra -Werror -shared -fPIC TESTS/WAVSINK.C -ldl -o BUILD/WAVSINK/WAVSINK.SO
cc -x c -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Werror TESTS/WAVCHECK.C -o BUILD/WAVSINK/WAVCHECK
python3 - "${1:-WAVCHECK}" <<'PY'
import os, subprocess, json, hashlib
import re, sys
from pathlib import Path
tag=sys.argv[1]
if not re.fullmatch(r'[A-Z0-9]{1,8}',tag): raise SystemExit('Invalid report tag')
root=Path.cwd(); out=root/'BUILD/WAVSINK'; report=root/'RESULTS'/(tag+'.JSN')
if report.exists(): raise SystemExit('Preserve the previous observation-helper test')
args=[str(out/'WAVCHECK'),str(out/'SELECT.WAV'),str(out/'OTHER.WAV')]
base=subprocess.run(args,check=True,capture_output=True,text=True)
proof=root/'RESULTS'/(tag+'.TXT')
if proof.exists(): raise SystemExit('Preserve the previous activation proof')
env=dict(os.environ,LD_PRELOAD=str(out/'WAVSINK.SO'),RP_WAV_PATH=args[1],RP_WAV_PROOF=str(proof))
test=subprocess.run(args,env=env,check=True,capture_output=True,text=True)
duplicate=subprocess.run(args,env=env,capture_output=True,text=True)
passed=(base.stdout=='selected=0 other=0\n' and test.stdout=='selected=4 other=0\n'
        and duplicate.returncode==125 and 'STDIO_BUFFER=0' in proof.read_text())
report.write_text(json.dumps({'scope':'Host-only stream buffering control; no timing or playback acceptance claim.',
 'baseline':base.stdout,'selected_unbuffered':test.stdout,'notice':test.stderr,
 'activation_proof':proof.name,'duplicate_proof_exit':duplicate.returncode,
 'helper_sha256':hashlib.sha256((out/'WAVSINK.SO').read_bytes()).hexdigest(),
 'result':'PASS' if passed else 'FAIL'},indent=2)+'\n')
print('WAV observation helper', 'PASS' if passed else 'FAIL')
raise SystemExit(not passed)
PY
