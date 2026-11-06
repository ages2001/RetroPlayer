"""Stage the installer on a cleanly stopped disposable test disk."""
import argparse
from pathlib import Path
import runpy
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('NT31', 'NT351', 'NT40', 'W95'))
a = p.parse_args()
setup = root/'BUILD/SETUP'
runpy.run_path(str(root/'SCRIPTS/AUDIT.PY'))['audit_names'](setup)
if not (setup/'FILES.DAT').is_file() or not (setup/'INSTALL.EXE').is_file():
    raise SystemExit('Build and package the installer first')
subprocess.run([sys.executable, str(root/'SCRIPTS/VMIO.PY'), a.guest], check=True)
stem = 'N351' if a.guest == 'NT351' else a.guest
disk = str(root/'TESTDISK'/(stem+'EDIT.IMG'))+'@@32256'
def directory(path):
    if subprocess.run(['mdir', '-i', disk, path+'/'], capture_output=True).returncode:
        subprocess.run(['mmd', '-i', disk, path], check=True)
directory('::/RPSETUP')
for path in sorted(setup.rglob('*')):
    target = '::/RPSETUP/'+str(path.relative_to(setup))
    if path.is_dir():
        directory(target)
    elif path.is_file():
        subprocess.run(['mcopy', '-o', '-i', disk, str(path), target], check=True)
print('Staged installer at C:\\RPSETUP on', a.guest)
