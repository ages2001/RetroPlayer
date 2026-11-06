"""Run a command from the test desktop. Close modal dialogs/Notepad first.

NT3.x requires Program Manager focus; NT4/Win95 use the global Start menu.
Wait for modifier release before typing: HMP's default 100 ms key hold can
otherwise turn the first command characters into unintended Ctrl shortcuts.
"""
import runpy
import subprocess
import sys
import time
from pathlib import Path

root = Path(__file__).resolve().parent
Monitor = runpy.run_path(str(root/'QMP.PY'))['Monitor']
target, command = sys.argv[1:]
if target not in ('NT31', 'NT351', 'NT40', 'W95', 'W2K', 'XP64'):
    raise SystemExit('Unknown guest')
socket = '/tmp/'+target+'.SOC'
monitor = Monitor(socket)
try:
    for key in (('alt-f', 'r') if target in ('NT31', 'NT351') else ('ctrl-esc', 'r')):
        monitor.call('human-monitor-command', **{'command-line': 'sendkey '+key+' 60'})
        time.sleep(1)
    monitor.call('human-monitor-command', **{'command-line': 'sendkey ctrl-a 30'})
    time.sleep(.2)
finally:
    monitor.close()
subprocess.run([sys.executable, str(root/'TYPE.PY'), socket, command+'\n'], check=True)
