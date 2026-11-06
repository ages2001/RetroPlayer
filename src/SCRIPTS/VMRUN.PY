"""Control the private test harness over a local QEMU serial socket."""
import argparse
from pathlib import Path
import re
import socket
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('guest',choices=('NT31','NT351','NT40','W95'))
p.add_argument('command',choices=('PING','STAT','RUN','GET','QUIT','OFF'))
p.add_argument('name',nargs='?')
p.add_argument('--output',type=Path)
a=p.parse_args()
if a.command in ('RUN','GET'):
    if not a.name or not re.fullmatch(r'[A-Z0-9_]{1,8}\.[A-Z0-9_]{1,3}',a.name):
        p.error('A DOS 8.3 uppercase filename is required')
elif a.name: p.error('Unexpected filename')
if a.command=='GET' and not a.output: p.error('GET requires --output')
if a.output:
    reports=Path(__file__).resolve().parent.parent/'RESULTS'
    if a.output.resolve().parent!=reports or not re.fullmatch(r'[A-Z0-9_]{1,8}\.[A-Z0-9_]{1,3}',a.output.name):
        p.error('Output must be a DOS 8.3 file inside RESULTS')
with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as sock:
    sock.settimeout(30)
    sock.connect('/tmp/'+a.guest+'.SER')
    with sock.makefile('rwb',buffering=0) as stream:
        # The socket can inject bytes faster than a legacy guest services its
        # 16-byte UART FIFO. With no hardware flow control, a longer GET can
        # lose its final newline and leave the agent waiting indefinitely.
        # Pace this small command; report payloads retain their checksums.
        request=(a.command+(' '+a.name if a.name else '')+'\n').encode('ascii')
        for byte in request:
            stream.write(bytes((byte,)))
            time.sleep(.005)
        reply=stream.readline(128).decode('ascii').strip()
        if a.command!='GET': print(reply)
        else:
            match=re.fullmatch(r'DATA (\d+) ([0-9a-fA-F]{8})',reply)
            if not match: raise SystemExit(reply)
            size=int(match[1]);expected=int(match[2],16)
            if size>8*1024*1024: raise SystemExit('Report exceeds transfer limit')
            data=bytearray()
            while len(data)<size:
                block=stream.read(min(16384,size-len(data)))
                if not block: raise SystemExit('Incomplete report')
                data.extend(block)
            if stream.readline(8)!=b'\n' or stream.readline(8)!=b'END\n':
                raise SystemExit('Missing report terminator')
            value=2166136261
            for byte in data: value=((value^byte)*16777619)&0xffffffff
            if value!=expected: raise SystemExit('Report checksum mismatch')
            a.output.write_bytes(data)
            print(str(a.output)+': '+str(size)+' bytes verified')
        if reply.startswith('ERROR'): raise SystemExit(1)
