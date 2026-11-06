#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD /workspace/RPLIB/lib /workspace/RPLIB/bin
python3 - <<'PY'
from pathlib import Path
lines = ['LIBRARY RPATOM.DLL', 'EXPORTS']
for n in (1, 2, 4, 8):
    for name in ('load', 'store', 'exchange'):
        lines.append(f' __atomic_{name}_{n}=rp_{name}{n}')
    lines.append(f' __atomic_compare_exchange_{n}=rp_compare{n}')
    for op in ('add', 'sub', 'and', 'or', 'xor', 'nand'):
        lines.append(f' __atomic_fetch_{op}_{n}=rp_fetch_{op}{n}')
        lines.append(f' __atomic_{op}_fetch_{n}=rp_{op}_fetch{n}')
        lines.append(f' __sync_fetch_and_{op}_{n}=rp_sync_fetch_{op}{n}')
        lines.append(f' __sync_{op}_and_fetch_{n}=rp_sync_{op}_fetch{n}')
    for public, private in [('val_compare_and_swap','val_compare'),
        ('bool_compare_and_swap','bool_compare'), ('lock_test_and_set','lock'),
        ('lock_release','unlock')]:
        lines.append(f' __sync_{public}_{n}=rp_sync_{private}{n}')
lines.append(' __sync_synchronize=rp_sync_barrier')
for public, private in [('load','load'), ('store','store'), ('exchange','exchange'),
    ('compare_exchange','compare'), ('is_lock_free','lock_free'),
    ('test_and_set','test_set'), ('clear','clear'), ('thread_fence','fence'),
    ('signal_fence','fence')]:
    lines.append(f' __atomic_{public}=rp_{private}')
Path('BUILD/ATOM.DEF').write_text('\n'.join(lines)+'\n')
PY
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mtune=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
  -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -c SRC/ATOM.C -o BUILD/ATOM.O
i686-w64-mingw32-gcc -shared -nostdlib -o /workspace/RPLIB/bin/RPATOM.DLL \
  BUILD/ATOM.O BUILD/ATOM.DEF -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp,--out-implib,/workspace/RPLIB/lib/atom.a -lkernel32
python3 SCRIPTS/AUDIT.PY /workspace/RPLIB/bin
