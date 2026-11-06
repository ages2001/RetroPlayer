"""Local QEMU monitor client. Does not open network listeners."""
import argparse
import json
import socket


class Monitor:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.settimeout(10)
        self.sock.connect(path)
        self.stream = self.sock.makefile('rwb')
        self.read()
        self.call('qmp_capabilities')

    def read(self):
        line = self.stream.readline()
        if not line:
            raise RuntimeError('QEMU disconnected')
        return json.loads(line)

    def call(self, command, **arguments):
        self.stream.write((json.dumps({'execute': command,
                                      'arguments': arguments})+'\n').encode())
        self.stream.flush()
        while True:
            reply = self.read()
            if 'error' in reply:
                raise RuntimeError(reply['error'])
            if 'return' in reply:
                return reply['return']

    def close(self):
        self.stream.close()
        self.sock.close()


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('socket')
    p.add_argument('command')
    p.add_argument('arguments', nargs='?', default='{}')
    a = p.parse_args()
    m = Monitor(a.socket)
    try:
        print(json.dumps(m.call(a.command, **json.loads(a.arguments))))
    finally:
        m.close()
