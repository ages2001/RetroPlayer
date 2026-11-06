#!/bin/sh
# Instrument a private copy of the actual calibration header for native logs.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/CLKPROBE
python3 - <<'PY'
from pathlib import Path
text=Path('/workspace/VLCWORK/src/win32/RPCLOCK.H').read_text()
needle='        ticks[i] = rp_tsc_read();'
assert text.count(needle)==1
text=text.replace(needle,needle+'\n        diag_ticks[i]=ticks[i];diag_ms[i]=stamps[i];diag_count=i+1;')
text=text.replace('return false;', 'return RPFail(__LINE__);')
Path('BUILD/CLKPROBE/RPCLOCK.H').write_text(text)
PY
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -Wno-cast-function-type \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
 -mcrtdll=crtdll -I BUILD/CLKPROBE \
 -c TESTS/CLKPROBE.C -o BUILD/CLKPROBE/CLKPROBE.O
./GCC386.SH -nostdlib -mcrtdll=crtdll \
 -o BUILD/CLKPROBE/CLKPROBE.EXE BUILD/CLKPROBE/CLKPROBE.O \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
 -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp \
 -lkernel32 -luser32 -lcrtdll -lgcc
python3 SCRIPTS/AUDIT.PY BUILD/CLKPROBE > RESULTS/CLKPROBE.JSN
