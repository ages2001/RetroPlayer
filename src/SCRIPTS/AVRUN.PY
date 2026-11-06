"""Observe actual guest video markers against the sound-file sample cursor.

Run after VMAGENT is ready, with other CPU-intensive guests stopped. The
audio capture still needs a clean guest shutdown and AVCHK analysis. A guest
process returning success is not an audiovisual synchronization result.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import runpy
import socket
import struct
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
Monitor = runpy.run_path(str(root/'SCRIPTS/QMP.PY'))['Monitor']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('guest', choices=('NT31', 'NT351', 'NT40', 'W95'))
p.add_argument('--tag', default='A1')
p.add_argument('--device', choices=('hda', 'sb16'), default='hda',
               help='Select the capture from an already configured VM; does not change guest hardware')
p.add_argument('--avformat', action='store_true', help='Use the explicit avformat diagnostic harness')
p.add_argument('--fixed-aac', action='store_true', help='Use the explicit integer AAC diagnostic harness')
p.add_argument('--lead', action='store_true', help='Observe all 80 timed transitions after one second of black/silence per fixture')
p.add_argument('--perf', action='store_true', help='AV2 diagnostic with the performance clock and a 5 ms timer request')
p.add_argument('--sample-ms', type=int, default=20, help='Screenshot sampling interval (10 to 100 ms)')
a = p.parse_args()
if not re.fullmatch('[A-Z0-9]{2}', a.tag):
    p.error('Tag must have two uppercase letters/digits')
if not 10 <= a.sample_ms <= 100:
    p.error('Sampling interval must be between 10 and 100 ms')
if sum((a.avformat, a.fixed_aac, a.lead, a.perf)) > 1:
    p.error('Select at most one diagnostic override')
prefix = {'NT31': 'N31', 'NT351': 'N351', 'NT40': 'N40', 'W95': 'W95'}[a.guest]+a.tag
capture = root/'RESULTS'/(a.guest+('SND.WAV' if a.device == 'hda' else 'SB.WAV'))
program = 'AVPERF.EXE' if a.perf else 'AV2TEST.EXE' if a.lead else 'AVFTEST.EXE' if a.avformat else 'AVFIX.EXE' if a.fixed_aac else 'AVTEST.EXE'
harness_dir = root/('BUILD/AVPERF' if a.perf else 'BUILD/AV2' if a.lead else 'BUILD/ASM')
fixture_manifest = root/('BUILD/AV2MEDIA/REFS.JSN' if a.lead or a.perf else 'BUILD/ASM/AVREFS.JSN')
if (root/'RESULTS'/(prefix+'TR.JSN')).exists():
    p.error('Choose a new tag; prior output measurements are preserved')
def command(*args):
    return subprocess.check_output([sys.executable, str(root/'SCRIPTS/VMRUN.PY'), a.guest, *args], text=True).strip()
def run(program):
    if command('RUN', program) != 'STARTED':
        raise RuntimeError('Could not start '+program)
def await_exit(timeout):
    deadline = time.monotonic()+timeout
    while time.monotonic()<deadline:
        state = command('STAT')
        if state != 'BUSY':
            return state
        time.sleep(.5)
    raise TimeoutError('Guest process timed out')
def wave_info():
    with capture.open('rb') as stream:
        if stream.read(4) != b'RIFF':
            raise ValueError('Not a RIFF capture')
        stream.read(4)
        if stream.read(4) != b'WAVE':
            raise ValueError('Not a WAVE capture')
        info = {}
        while True:
            header = stream.read(8)
            if len(header) != 8:
                raise ValueError('Missing audio data chunk')
            tag, size = struct.unpack('<4sI', header)
            if tag == b'fmt ':
                data = stream.read(size)
                encoding, channels, rate, _, align, bits = struct.unpack('<HHIIHH', data[:16])
                if encoding != 1 or bits != 16 or not channels or align != channels*2:
                    raise ValueError('Expected 16-bit PCM capture')
                info.update(rate=rate, channels=channels, align=align)
            elif tag == b'data':
                if not info:
                    raise ValueError('Audio format is missing')
                info['offset'] = stream.tell()
                return info
            else:
                stream.seek(size, 1)
            if size & 1:
                stream.seek(1, 1)
def marker(path):
    data = path.read_bytes()
    header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\s', data)
    if not header:
        raise ValueError('Unexpected QEMU screenshot format')
    width, height = map(int, header.groups())
    pixels = memoryview(data)[header.end():]
    if len(pixels) != width*height*3 or width < 464 or height < 348:
        raise ValueError('Unexpected screenshot dimensions')
    bits = []
    for cell in range(8):
        # Sample several interior pixels, away from scaling/filter boundaries.
        values = []
        for dy in (-2, 0, 2):
            for dx in (-2, 0, 2):
                offset = ((60+144+dy)*width+80+24+48*cell+dx)*3
                values.append(sum(pixels[offset:offset+3]))
        average = sum(values)/len(values)
        if average > 650:
            bits.append(1)
        elif average < 100:
            bits.append(0)
        else:
            return None
    scene = sum(bit << shift for bit, shift in zip(bits[2:], range(5, -1, -1)))
    return [bits[1], scene] if bits[0] and scene < 40 else None

def sink_buffering():
    # A normal capture keeps the historical conservative stdio bound.
    # Zero is allowed only for the helper actually active in this QEMU.
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            args = (proc/'cmdline').read_bytes().split(b'\0')
            if not args or b'qemu-system' not in args[0] or not any(
                    ('path='+str(capture)).encode() in x for x in args):
                continue
            env = dict(x.split(b'=', 1) for x in (proc/'environ').read_bytes().split(b'\0') if b'=' in x)
            if env.get(b'RP_WAV_PATH') != str(capture).encode():
                return {'stdio_buffer_bytes': 8192}
            proof = Path(env[b'RP_WAV_PROOF'].decode())
            if proof.parent != root/'RESULTS' or not re.fullmatch(r'[A-Z0-9]{1,8}\.TXT',proof.name):
                raise RuntimeError('Invalid WAV activation proof path')
            raw = proof.read_bytes()
            expected = ('PID='+proc.name+'\nCAPTURE='+str(capture)+'\nSTDIO_BUFFER=0\n').encode()
            helper = root/'BUILD/WAVSINK/WAVSINK.SO'
            if raw != expected or str(helper) not in (proc/'maps').read_text():
                raise RuntimeError('Unbuffered WAV activation was not verified')
            return {'stdio_buffer_bytes': 0, 'activation': {'pid': int(proc.name),
                    'proof': proof.name, 'proof_sha256': hashlib.sha256(raw).hexdigest(),
                    'helper_sha256': hashlib.sha256(helper.read_bytes()).hexdigest()}}
        except (FileNotFoundError, ProcessLookupError):
            continue
    return {'stdio_buffer_bytes': 8192}

synchronized = False
for attempt in range(4):
    try:
        with socket.socket(socket.AF_UNIX) as sock:
            sock.settimeout(3)
            sock.connect('/tmp/'+a.guest+'.SER')
            with sock.makefile('rwb', buffering=0) as stream:
                stream.write(b'\n');time.sleep(.2);stream.write(b'PING\n')
                for _ in range(8):
                    if stream.readline(128) == b'PONG\n':
                        synchronized = True
                        break
    except TimeoutError:
        pass
    if synchronized:
        break
if not synchronized:
    raise RuntimeError('Guest serial synchronization failed')
print(a.guest, 'preparing plugin cache', flush=True)
run('CACHE.EXE')
if await_exit(180) != 'EXIT 0':
    raise RuntimeError('Plugin cache generation failed')
info = wave_info()
info.update(sink_buffering())
trace = {'guest': a.guest, 'device': a.device, 'scope': __doc__, 'audio': info,
         'demux_override': 'avformat' if a.avformat else None,
         'aac_override': 'aac_fixed' if a.fixed_aac else None,
         'harness': program,
         'host_harness_sha256': hashlib.sha256((harness_dir/program).read_bytes()).hexdigest(),
         'fixture_lead_seconds': int(a.lead or a.perf),
         'clock_override': 'perf' if a.perf else None,
         'host_fixtures': json.loads(fixture_manifest.read_text()),
         'sampling_interval_ms': a.sample_ms,
         'source_capture': capture.name, 'samples': [], 'result': 'INCOMPLETE'}
# Keep the 50 Hz QMP screenshots off disk-backed /tmp: synchronous file
# writes run in QEMU's main loop and can delay the emulated sound device.
screen = Path('/dev/shm' if Path('/dev/shm').is_dir() else '/tmp')/(a.guest+'AV.PPM')
monitor = Monitor('/tmp/'+a.guest+'.SOC')
trace['guest_memory'] = monitor.call('query-memory-size-summary')
poller = concurrent.futures.ThreadPoolExecutor(max_workers=1)
pending = None
report_path = root/'RESULTS'/(prefix+'TR.JSN')
try:
    print(a.guest, 'starting audiovisual observation', flush=True)
    run(program)
    deadline = time.monotonic()+190
    next_poll = next_update = time.monotonic()
    saved = set()
    state = 'BUSY'
    while time.monotonic()<deadline:
        before = max(0, capture.stat().st_size-info['offset'])//info['align']
        start = time.monotonic()
        monitor.call('screendump', filename=str(screen))
        end = time.monotonic()
        after = max(0, capture.stat().st_size-info['offset'])//info['align']
        label = marker(screen)
        trace['samples'].append({'before': before, 'after': after,
                                 'snapshot_seconds': end-start, 'marker': label})
        if label is not None and label[1] in (0, 20, 39) and tuple(label) not in saved:
            saved.add(tuple(label))
            destination = root/'RESULTS'/(prefix+str(label[0])+str((0,20,39).index(label[1]))+'.PPM')
            destination.write_bytes(screen.read_bytes())
        # Serial replies on old guests can take seconds. Never let one
        # suspend screenshot sampling and hide intervening scene changes.
        if pending is not None and pending.done():
            state = pending.result()
            pending = None
            next_poll = end+1
            if state != 'BUSY':
                break
        if pending is None and end >= next_poll:
            pending = poller.submit(command, 'STAT')
        if end >= next_update:
            print(a.guest, 'observations', len(trace['samples']), 'marker', label, flush=True)
            next_update = end+10
        time.sleep(max(0, a.sample_ms/1000-(time.monotonic()-start)))
    trace['guest_exit'] = state
    if pending is not None:
        pending.result()
    for name, suffix in [('AVTEST.TXT', 'AV'), ('AVLOG.TXT', 'AL')]:
        command('GET', name, '--output', str(root/'RESULTS'/(prefix+suffix+'.TXT')))
    trace['result'] = 'CAPTURED' if state == 'EXIT 0' else 'FAIL'
finally:
    poller.shutdown(wait=True)
    monitor.close()
    report_path.write_text(json.dumps(trace, indent=2)+'\n')
print('Saved', report_path.name, '; analyze after closing the sound capture', flush=True)
raise SystemExit(trace['result'] != 'CAPTURED')
