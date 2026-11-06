"""Run reproducible guest regressions through the private serial harness."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import re
import socket
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('ALL', 'NT31', 'NT351', 'NT40', 'W95'))
p.add_argument('--tag', required=True, help='Two-character report revision, for example X5')
p.add_argument('--suite', action='store_true')
p.add_argument('--install', action='store_true')
p.add_argument('--guard', action='store_true', help='Enumerate plugins with the existing PE import preflight')
p.add_argument('--audio', action='store_true')
p.add_argument('--video', action='store_true')
p.add_argument('--ui', action='store_true')
p.add_argument('--queue', action='store_true')
p.add_argument('--resampler', action='store_true')
p.add_argument('--priority', action='store_true')
p.add_argument('--waits', action='store_true', help='Exercise actual core condition waits and its first clock read')
p.add_argument('--only', choices=('audio', 'video', 'install', 'resampler', 'priority', 'queue', 'suite', 'ui', 'gdi', 'waits'), help='Run only the selected regression; media tests generate the cache first')
p.add_argument('--device', choices=('hda', 'sb16'), default='hda')
p.add_argument('--parallel', action='store_true', help='Diagnostic stress mode; may cause real-time underruns')
a = p.parse_args()
if not re.fullmatch('[A-Z0-9]{2}', a.tag):
    p.error('Tag must have two uppercase letters/digits')
prefixes = {'NT31': 'N31', 'NT351': 'N351', 'NT40': 'N40', 'W95': 'W95'}
requested_guests = list(prefixes) if a.guest == 'ALL' else [a.guest]
for guest in requested_guests:
    prefix = prefixes[guest] + a.tag
    if any((root/'RESULTS').glob(prefix + '*')):
        p.error('Choose a new tag; prior reports are preserved: ' + prefix)

def run(guest):
    prefix = prefixes[guest]+a.tag
    def command(*args):
        return subprocess.check_output([sys.executable, str(root/'SCRIPTS/VMRUN.PY'),
                                        guest, *args], text=True).strip()
    # A connection opened during guest UART initialization can leave a partial
    # line. Synchronize with PING before sending any test command.
    synchronized = False
    for attempt in range(4):
        try:
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(3)
                sock.connect('/tmp/'+guest+'.SER')
                with sock.makefile('rwb', buffering=0) as stream:
                    stream.write(b'\n')
                    time.sleep(.2)
                    stream.write(b'PING\n')
                    for _ in range(8):
                        if stream.readline(128) == b'PONG\n':
                            synchronized = True
                            break
        except TimeoutError:
            pass
        if synchronized:
            break
    if not synchronized:
        raise RuntimeError(guest+': serial synchronization failed')
    jobs = [] if a.only in ('install', 'resampler', 'priority', 'suite', 'waits') else [('CACHE.EXE', [('CACHE.TXT', 'CA')])]
    if a.waits or a.only == 'waits':
        jobs.append(('CWTEST.EXE', [('CWTEST.TXT', 'CW')]))
    if not a.only:
        jobs.append(('AFMTEST.EXE', [('AFMTEST.TXT', 'AF'), ('AFMLOG.TXT', 'AL')]))
    if a.video or a.only == 'video':
        jobs.append(('VFMTEST.EXE', [('VFMTEST.TXT', 'VF'), ('VFMLOG.TXT', 'VL')]))
    if a.suite or a.only == 'suite':
        jobs.append(('TXTTRY.EXE', [('SUITE.TXT', 'SU'), ('CRTEST.TXT', 'CR')]))
    if a.install or a.only == 'install':
        jobs.append(('INSTEST.EXE', [('INSTEST.TXT', 'IN')]))
    if a.resampler or a.only == 'resampler':
        jobs.append(('RESAMP.EXE', [('RESAMP.TXT', 'RS')]))
    if a.priority or a.only == 'priority':
        jobs.append(('PRITEST.EXE', [('PRITEST.TXT', 'PR')]))
        jobs.append(('ATTEST.EXE', [('ATTEST.TXT', 'AT')]))
    if a.ui or a.only == 'ui':
        jobs.append(('UITEST.EXE', [('UITEST.TXT', 'UI')]))
    if a.only == 'gdi':
        jobs.append(('GDISUB.EXE', [('GDISUB.TXT', 'GS')]))
    if a.guard:
        jobs.append(('GUARD.EXE', [('PLOAD.TXT', 'PG')]))
    if a.queue or a.only == 'queue':
        jobs.append(('QUTEST.EXE', [('QUTEST.TXT', 'QU'), ('QULOG.TXT', 'QL')]))
    if a.audio or a.only == 'audio':
        jobs.append(('AOUTTEST.EXE', [('AOUTTEST.TXT', 'AU'), ('AOUTLOG.TXT', 'AO')]))
    results = {}
    report_errors = {}
    for program, reports in jobs:
        if program in ('QUTEST.EXE', 'AOUTTEST.EXE'):
            capture = root/'RESULTS'/(guest+('SND.WAV' if a.device == 'hda' else 'SB.WAV'))
            queue_before = max(0, capture.stat().st_size-44)//4
        if command('RUN', program) != 'STARTED':
            raise RuntimeError(guest+': test did not start: '+program)
        print(guest, program, 'STARTED', flush=True)
        deadline = time.monotonic()+900
        while time.monotonic() < deadline:
            status = command('STAT')
            if status != 'BUSY':
                break
            time.sleep(2)
        else:
            raise TimeoutError(guest+': test still running: '+program)
        results[program] = status
        (root/'RESULTS'/(prefix+'.JSN')).write_text(json.dumps(results, indent=2)+'\n')
        if program == 'QUTEST.EXE':
            queue_after = max(0, capture.stat().st_size-44)//4
            (root/'RESULTS'/(prefix+'QB.JSN')).write_text(json.dumps({
                'scope': 'Observe short-clip test interval in QEMU 44100 Hz stereo S16 capture; analyze after clean shutdown.',
                'guest': guest, 'device': a.device, 'capture': capture.name,
                'before': queue_before, 'after': queue_after, 'guest_exit': status,
                'durations_ms': [5, 20, 70, 100, 200]}, indent=2)+'\n')
        elif program == 'AOUTTEST.EXE':
            audio_after = max(0, capture.stat().st_size-44)//4
            (root/'RESULTS'/(prefix+'AB.JSN')).write_text(json.dumps({
                'scope': 'Observe AOUTTEST alone; earlier QPACK tones must not count as this test.',
                'guest': guest, 'device': a.device, 'capture': capture.name,
                'before': queue_before, 'after': audio_after,
                'guest_exit': status}, indent=2)+'\n')
        for source, suffix in reports:
            try:
                command('GET', source, '--output', str(root/'RESULTS'/(prefix+suffix+'.TXT')))
            except subprocess.CalledProcessError as error:
                report_errors[source] = error.output.strip() if error.output else 'Report transfer failed'
                print(guest, source, 'REPORT FAILED', flush=True)
        print(guest, program, status, flush=True)
    (root/'RESULTS'/(prefix+'.JSN')).write_text(json.dumps(results, indent=2)+'\n')
    if report_errors:
        (root/'RESULTS'/(prefix+'ER.JSN')).write_text(json.dumps(report_errors, indent=2)+'\n')
    return not report_errors and all(value == 'EXIT 0' for value in results.values())

guests = list(prefixes) if a.guest == 'ALL' else [a.guest]
with concurrent.futures.ThreadPoolExecutor(len(guests) if a.parallel else 1) as executor:
    passed = list(executor.map(run, guests))
raise SystemExit(not all(passed))
