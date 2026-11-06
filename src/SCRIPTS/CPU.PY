"""Scan disassembled code for known instructions newer than the i386 baseline.

This is a conservative build diagnostic, not a proof of instruction coverage.
Runtime-dispatched optimizations require separate review and a tested guard.
"""
import argparse
import json
import re
import subprocess


FORBIDDEN = re.compile(
    r'^(?:cmov.*|fcmov.*|fcomi.*|fucomi.*|cmpxchg.*|xadd.*|bswap|'
    r'popcnt.*|lzcnt.*|tzcnt.*|pause|ud2|syscall|sysenter|sysexit|'
    r'rdtscp|rdrand|rdseed|xgetbv|xsetbv|fxsave.*|fxrstor.*|'
    r'xsave.*|xrstor.*|lfence|mfence|sfence|clflush.*|prefetch.*)$')
REGISTERS = re.compile(r'%(?:[xyz]mm\d+|mm[0-7])\b')
SYMBOL = re.compile(r'^[0-9a-f]+ <(.+)>:$')
INSTRUCTION = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(\S+)(.*)$')


def scan(path, objdump):
    output = subprocess.check_output([objdump, '-d', str(path)], text=True)
    symbol = None
    findings = []
    traps = []
    for line in output.splitlines():
        match = SYMBOL.match(line)
        if match:
            symbol = match[1]
            continue
        match = INSTRUCTION.match(line)
        if not match:
            continue
        address, _, mnemonic, operands = match.groups()
        # objdump prints a LOCK prefix as the mnemonic.
        if mnemonic == 'lock':
            fields = operands.strip().split(None, 1)
            mnemonic = fields[0]
            operands = fields[1] if len(fields) > 1 else ''
        # UD2 deliberately raises #UD on i386 too; it is not a CPU dispatch
        # requirement. Preserve it in the report as an intentional trap.
        if mnemonic == 'ud2':
            traps.append({'address': address, 'symbol': symbol})
            continue
        if FORBIDDEN.match(mnemonic) or REGISTERS.search(operands):
            findings.append({'address': address, 'symbol': symbol,
                             'instruction': mnemonic + operands})
    return {'file': str(path), 'findings': findings, 'intentional_traps': traps}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--objdump', default='i686-w64-mingw32-objdump')
    p.add_argument('files', nargs='+')
    a = p.parse_args()
    results = [scan(path, a.objdump) for path in a.files]
    print(json.dumps({'scope': __doc__, 'modules': results}, indent=2))
    raise SystemExit(1 if any(result['findings'] for result in results) else 0)
