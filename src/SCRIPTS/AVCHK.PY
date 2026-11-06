"""Compare observed guest GDI scene changes with closed emulated audio output.

This is an emulated-output timing check, not a physical display/speaker test.
The observation interval and a conservative stdio/sink allowance are included
in the reported bounds. A missing scene or pulse cannot be a timing pass.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import wave

import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('trace', type=Path)
p.add_argument('capture', type=Path)
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
if a.output.resolve().parent != root/'RESULTS' or not re.fullmatch(
        r'[A-Z0-9_]{1,8}\.JSN', a.output.name):
    p.error('Output must be an 8.3 JSON report inside RESULTS')
trace = json.loads(a.trace.read_text())
with wave.open(str(a.capture), 'rb') as stream:
    rate, channels = stream.getframerate(), stream.getnchannels()
    if stream.getsampwidth() != 2 or channels != 2 or rate != trace['audio']['rate']:
        raise SystemExit('Unexpected capture format')
    samples = np.frombuffer(stream.readframes(stream.getnframes()), '<i2').reshape(-1, channels)
mono = samples.astype(np.float64).mean(axis=1)
failures = []
if trace.get('result') != 'CAPTURED' or trace.get('guest_exit') != 'EXIT 0':
    failures.append('Guest execution or observation did not complete')
observations = trace['samples']
if len(observations) < 2:
    raise SystemExit('No observation intervals')
if any(x['before'] > x['after'] or x['before'] < 0 or x['after'] > len(mono)
       for x in observations):
    raise SystemExit('Invalid capture cursor')
if any(y['before'] < x['before'] for x, y in zip(observations, observations[1:])):
    raise SystemExit('Capture cursor moved backwards')

# A 4 ms RMS window averages several cycles. Merge <= 12 ms dropouts but
# retain the surrounding 400 ms silence between the individual markers.
step = max(1, rate//250)
start = max(0, observations[0]['before']-rate//2)
end = min(len(mono), observations[-1]['after']+rate//2)
count = (end-start)//step
rms = np.sqrt(np.mean(mono[start:start+count*step].reshape(-1, step)**2, axis=1))
active = np.flatnonzero(rms > 500)
groups = []
for index in active:
    if not groups or index-groups[-1][-1] > 4:
        groups.append([int(index)])
    else:
        groups[-1].append(int(index))
pulses = []
for group in groups:
    lo, hi = start+group[0]*step, start+(group[-1]+1)*step
    if hi-lo < rate*.025:
        failures.append('Unexpected short audio pulse at %.3f s' % (lo/rate))
        continue
    signal = mono[lo:hi]
    spectrum = np.abs(np.fft.rfft((signal-signal.mean())*np.hanning(len(signal)), n=65536))
    frequencies = np.fft.rfftfreq(65536, 1/rate)
    band = (frequencies >= 350) & (frequencies <= 850)
    frequency = float(frequencies[band][np.argmax(spectrum[band])])
    fixture = min((0, 1), key=lambda n: abs(frequency-(441, 735)[n]))
    if abs(frequency-(441, 735)[fixture]) > 20:
        failures.append('Unexpected pulse frequency %.2f Hz' % frequency)
    pulses.append({'fixture': fixture, 'start': lo, 'end': hi,
                   'frequency_hz': round(frequency, 3), 'duration_ms': (hi-lo)*1000/rate})

# QEMU's WAV sink uses fwrite. glibc's stdio buffer is at most BUFSIZ (8192)
# on this host; add 20 ms for audio scheduling plus the RMS window. The
# screenshot interval itself is measured separately, not hidden in a limit.
stdio_bytes = trace['audio'].get('stdio_buffer_bytes', 8192)
if stdio_bytes not in (0, 8192):
    raise SystemExit('Unknown WAV stdio buffering bound')
if stdio_bytes == 0:
    activation = trace['audio'].get('activation', {})
    proof = root/'RESULTS'/activation.get('proof', '')
    if not proof.is_file() or hashlib.sha256(proof.read_bytes()).hexdigest() != activation.get('proof_sha256'):
        raise SystemExit('Missing or changed unbuffered WAV activation proof')
stdio_allowance = stdio_bytes/(channels*2*rate)
sink_allowance = .020+step/rate
limit = .150
transitions = {}
ordered = []
previous = None
for index, observation in enumerate(observations):
    label = observation['marker']
    if label is None:
        previous = None
        continue
    key = tuple(label)
    if key != previous:
        if key in transitions:
            # Short invalid screenshots may interrupt an unchanged scene.
            if not ordered or ordered[-1] != key:
                failures.append('Video scene reappeared out of order: %s' % (key,))
        else:
            earlier = observations[max(0, index-1)]
            transitions[key] = (earlier['before']/rate, observation['after']/rate)
            ordered.append(key)
    previous = key
expected = [(fixture, scene) for fixture in range(2) for scene in range(40)]
if ordered != expected:
    failures.append('Video scene sequence is incomplete or out of order')
if [pulse['fixture'] for pulse in pulses] != [fixture for fixture, scene in expected]:
    failures.append('Expected 40 ordered audio pulses for each fixture; found %d total' % len(pulses))
results = []
for fixture in range(2):
    audio = [pulse for pulse in pulses if pulse['fixture'] == fixture]
    for scene in range(40):
        row = {'fixture': fixture, 'scene': scene}
        if scene >= len(audio) or (fixture, scene) not in transitions:
            row['result'] = 'MISSING'
            results.append(row)
            continue
        pulse = audio[scene]
        before, after = transitions[(fixture, scene)]
        onset = pulse['start']/rate
        # Positive offset means audio follows the visible transition.
        # Buffered fwrite can only make the file cursor trail the output.
        lower = onset-after-stdio_allowance-sink_allowance
        upper = onset-before+sink_allowance
        duration_ok = 65 <= pulse['duration_ms'] <= 140
        spacing = None if not scene else (pulse['start']-audio[scene-1]['start'])/rate
        spacing_ok = spacing is None or .46 <= spacing <= .54
        ok = lower >= -limit and upper <= limit and duration_ok and spacing_ok
        row.update(audio=pulse, transition_interval_ms=[before*1000, after*1000],
                   audio_minus_video_ms=[lower*1000, upper*1000],
                   pulse_spacing_ms=None if spacing is None else spacing*1000,
                   result='PASS' if ok else 'FAIL')
        if not ok:
            failures.append('Fixture %d scene %d failed timing/duration/spacing' % (fixture, scene))
        results.append(row)
report = {'scope': __doc__, 'guest': trace['guest'], 'capture': a.capture.name,
          'capture_sha256': hashlib.sha256(a.capture.read_bytes()).hexdigest(),
          'trace': a.trace.name, 'trace_sha256': hashlib.sha256(a.trace.read_bytes()).hexdigest(),
          'sample_rate': rate, 'channels': channels, 'observation_count': len(observations),
          'tolerance_ms': limit*1000, 'stdio_lag_bound_ms': stdio_allowance*1000,
          'sink_and_detection_allowance_ms': sink_allowance*1000,
          'scenes': results, 'failures': failures, 'result': 'FAIL' if failures else 'PASS'}
a.output.write_text(json.dumps(report, indent=2)+'\n')
print(report['result'], 'pulses', len(pulses), 'video scenes', len(ordered), 'failures', len(failures))
for failure in failures[:12]:
    print(failure)
raise SystemExit(bool(failures))
