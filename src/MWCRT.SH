#!/bin/sh
# Rebuild the C support objects; binary toolchain libraries target newer CPUs.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
revision=819a6ec2ea87c19814b287e21d65e0dc7f05abba
if [ ! -d /workspace/MWREF/.git ]; then
  git clone --depth 1 --branch v12.0.0 https://github.com/mingw-w64/mingw-w64.git /workspace/MWREF
fi
test "$(git -C /workspace/MWREF rev-parse HEAD)" = "$revision"
test -z "$(git -C /workspace/MWREF status --porcelain)"
python3 - <<'PY'
from pathlib import Path
import subprocess
root = Path('/workspace/MWWORK')
if not root.exists():
    root.mkdir()
    with subprocess.Popen(['git', '-C', '/workspace/MWREF', 'archive', 'HEAD'], stdout=subprocess.PIPE) as p:
        subprocess.run(['tar', '-x', '-C', str(root)], stdin=p.stdout, check=True)
        if p.wait():
            raise SystemExit('Source export failed')

PY
patch_file="$PWD/PATCHES/MINGW.DIF"
if patch --dry-run --batch --forward -p1 -d /workspace/MWWORK < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 -d /workspace/MWWORK < "$patch_file"
elif ! patch --dry-run --batch --reverse -p1 -d /workspace/MWWORK < "$patch_file" >/dev/null 2>&1; then
  echo 'MinGW working source differs from both the reference and reviewed patch' >&2
  exit 2
fi
mkdir -p /workspace/MWBLD
cd /workspace/MWBLD
/workspace/MWWORK/mingw-w64-crt/configure --host=i686-w64-mingw32 \
  --prefix=/workspace/MW386 \
  --with-sysroot=/workspace/TOOLS/SYSROOT/usr/i686-w64-mingw32 \
  --disable-lib64 --enable-lib32 --with-default-msvcrt=crtdll \
  CFLAGS='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics'
make -j"${RP_JOBS:-4}" lib32/libmingw32.a lib32/libmingwex.a lib32/libmoldname.a \
  lib32/dllcrt2.o lib32/crt2.o lib32/libcrtdll.a
