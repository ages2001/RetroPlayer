#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD /workspace/RPLIB/bin /workspace/RPLIB/lib
python3 - <<'PY'
from pathlib import Path
functions = [('getaddrinfo','rp_getaddrinfo',16), ('freeaddrinfo','rp_freeaddrinfo',4),
    ('getnameinfo','rp_getnameinfo',28), ('WSASendTo','rp_sendto',36),
    ('WSARecvFrom','rp_recvfrom',36), ('WSARecv','rp_recv',28), ('WSAIoctl','rp_ioctl',36)]
exports=['LIBRARY RPNET.DLL','EXPORTS']
imports=['LIBRARY RPNET.DLL','EXPORTS']
for public,private,size in functions:
    exports.append(f' {public}={private}@{size}')
    imports.append(f' {public}@{size}')
Path('BUILD/NET.DEF').write_text('\n'.join(exports)+'\n')
Path('BUILD/NETIMP.DEF').write_text('\n'.join(imports)+'\n')
PY
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -c SRC/NET.C -o BUILD/NET.O
i686-w64-mingw32-gcc -shared -nostdlib -o /workspace/RPLIB/bin/RPNET.DLL \
  BUILD/NET.O BUILD/NET.DEF -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lwsock32 -lkernel32
i686-w64-mingw32-dlltool --kill-at --input-def BUILD/NETIMP.DEF \
  --output-lib /workspace/RPLIB/lib/net.a
python3 SCRIPTS/AUDIT.PY /workspace/RPLIB/bin
mkdir -p BUILD/NETTEST
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -c TESTS/NETTEST.C -o BUILD/NETTEST.O
i686-w64-mingw32-gcc -nostdlib -o BUILD/NETTEST/NETTEST.EXE BUILD/NETTEST.O \
  /workspace/RPLIB/lib/net.a -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lwsock32 -lkernel32 -luser32
cp /workspace/RPLIB/bin/RPNET.DLL BUILD/NETTEST/RPNET.DLL
python3 SCRIPTS/AUDIT.PY BUILD/NETTEST
