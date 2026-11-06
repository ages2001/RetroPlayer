"""Record the legacy MinGW CRT changes against its pinned clean source."""
import difflib
from pathlib import Path
import subprocess

reference = Path('/workspace/MWREF')
working = Path('/workspace/MWWORK')
project = Path(__file__).resolve().parent.parent
revision = '819a6ec2ea87c19814b287e21d65e0dc7f05abba'
if subprocess.check_output(['git', '-C', str(reference), 'rev-parse', 'HEAD'], text=True).strip() != revision:
    raise SystemExit('Unexpected MinGW revision')
if subprocess.check_output(['git', '-C', str(reference), 'status', '--porcelain']):
    raise SystemExit('Modified MinGW reference')
files = ['crt/crtdll.c', 'crt/crtexe.c', 'crt/tlssup.c', 'gdtoa/misc.c',
         'misc/invalid_parameter_handler.c', 'misc/output_format.c']
patch = []
for name in files:
    name = 'mingw-w64-crt/' + name
    patch.extend(difflib.unified_diff((reference/name).read_text().splitlines(keepends=True),
        (working/name).read_text().splitlines(keepends=True), fromfile='a/'+name, tofile='b/'+name))
(project/'PATCHES'/'MINGW.DIF').write_text(''.join(patch))
print('Recorded legacy CRT changes against '+revision)
