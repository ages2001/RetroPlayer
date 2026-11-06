#!/bin/sh
# GCC runtime sources authenticated by the signed Debian APT index.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
rp_deb=/workspace/TOOLS/DEBS/gcc-14-source_14.2.0-19_all.deb
if [ ! -f "$rp_deb" ]; then
  (cd /workspace/TOOLS/DEBS && APT_CONFIG=/workspace/TOOLS/APT/APT.CONF apt-get download gcc-14-source=14.2.0-19)
fi
printf '%s  %s\n' 44e6872715a380fe08418ea42b58d3c90de7d5dfd9f525bbf4dacba415c2266a "$rp_deb" | sha256sum -c -
dpkg-deb -x "$rp_deb" /workspace/TOOLS/SYSROOT
python3 - <<'PY'
from pathlib import Path
import shutil
import tarfile
reference=Path('/workspace/GCCREF')
working=Path('/workspace/GCCWORK')
if not reference.exists():
 reference.mkdir()
 with tarfile.open('/workspace/TOOLS/SYSROOT/usr/src/gcc-14/gcc-14.2.0-dfsg.tar.xz') as archive:
  for entry in archive:
   name=entry.name.removeprefix('gcc-14.2.0/')
   if not name or name.startswith(('gcc/testsuite/','libgfortran/','libgo/','libphobos/')): continue
   if name.startswith('libstdc++-v3/testsuite/') and name!='libstdc++-v3/testsuite/Makefile.in': continue
   entry.name=name
   archive.extract(entry,reference,filter='data')
if not working.exists(): shutil.copytree(reference,working,symlinks=True)
link=working/'libgcc/gthr-default.h'
if not link.exists(): link.symlink_to('config/i386/gthr-win32.h')
PY
if patch --dry-run -p1 -d /workspace/GCCWORK < PATCHES/GCC.DIF >/dev/null; then
  patch -p1 -d /workspace/GCCWORK < PATCHES/GCC.DIF
elif ! patch --dry-run -R -p1 -d /workspace/GCCWORK < PATCHES/GCC.DIF >/dev/null; then
  echo 'GCC working source differs from the reviewed patch' >&2; exit 1
fi
