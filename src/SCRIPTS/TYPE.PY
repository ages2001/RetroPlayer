"""Type ASCII text into a local test VM using QEMU key events."""
import sys
import time
import runpy
from pathlib import Path
Monitor = runpy.run_path(str(Path(__file__).with_name('QMP.PY')))['Monitor']

keys = {':': 'shift-semicolon', '\\': 'backslash', '/': 'slash',
        '.': 'dot', ' ': 'spc', '-': 'minus', '_': 'shift-minus',
        '\n': 'ret', '"': 'shift-apostrophe', ',': 'comma',
        ';': 'semicolon', '=': 'equal', '+': 'shift-equal',
        '~': 'shift-grave_accent', '@': 'shift-2', '*': 'shift-8',
        '?': 'shift-slash', '%': 'shift-5', '>': 'shift-dot',
        '<': 'shift-comma', '|': 'shift-backslash', '&': 'shift-7',
        '(': 'shift-9', ')': 'shift-0', "'": 'apostrophe'}
sequence = []
for char in sys.argv[2]:
    key = keys.get(char)
    if key is None:
        if char.isascii() and char.isalnum():
            key = ('shift-' if char.isupper() else '') + char.lower()
        else: raise ValueError(f'Unsupported key: {char!r}')
    sequence.append(key)
monitor = Monitor(sys.argv[1])
try:
    for key in sequence:
        monitor.call('human-monitor-command', **{'command-line': f'sendkey {key} 60'})
        time.sleep(0.2)
finally:
    monitor.close()
