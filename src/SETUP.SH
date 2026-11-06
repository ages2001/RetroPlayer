#!/bin/sh
# Workspace-local Debian tools, verified by APT; no root/system install.
set -eu
cd "$(dirname "$0")"
test -f /usr/share/keyrings/debian-archive-keyring.gpg
mkdir -p /workspace/TOOLS/APT/LISTS/partial /workspace/TOOLS/APT/CONF \
  /workspace/TOOLS/APT/CACHE/archives/partial /workspace/TOOLS/SYSROOT
cat > /workspace/TOOLS/APT/APT.CONF <<'EOF'
Dir::Etc::parts "/workspace/TOOLS/APT/CONF";
Dir::Etc::main "/workspace/TOOLS/APT/EMPTY";
Dir::Etc::sourcelist "/workspace/TOOLS/APT/PKGS.LST";
Dir::Etc::sourceparts "-";
Dir::State::lists "/workspace/TOOLS/APT/LISTS";
Dir::Cache "/workspace/TOOLS/APT/CACHE";
Dir::Cache::archives "/workspace/TOOLS/APT/CACHE/archives";
APT::Sandbox::User "root";
EOF
: > /workspace/TOOLS/APT/EMPTY
cat > /workspace/TOOLS/APT/PKGS.LST <<'EOF'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
EOF
export APT_CONFIG=/workspace/TOOLS/APT/APT.CONF
/usr/bin/apt-get update
/usr/bin/apt-get --download-only --no-install-recommends -y -o Debug::NoLocking=1 install \
  qemu-system-x86 qemu-utils gcc-mingw-w64-i686-win32 g++-mingw-w64-i686-win32 \
  binutils-mingw-w64-i686 mtools autoconf automake autopoint gettext \
  libtool libtool-bin libltdl-dev flex bison m4 gperf nasm meson ninja-build genisoimage
for archive in /workspace/TOOLS/APT/CACHE/archives/*.deb; do
  dpkg-deb -x "$archive" /workspace/TOOLS/SYSROOT
done
python3 - <<'PY'
from pathlib import Path
root = Path('/workspace/TOOLS/SYSROOT/usr')
for name, target in [('automake', 'automake-1.17'), ('aclocal', 'aclocal-1.17'),
    ('i686-w64-mingw32-gcc', 'i686-w64-mingw32-gcc-win32'),
    ('i686-w64-mingw32-g++', 'i686-w64-mingw32-g++-win32')]:
    path = root/'bin'/name
    if path.is_symlink(): path.unlink()
    if not path.exists(): path.symlink_to(target)
path = root/'share/libtool/m4'
if not path.exists(): path.symlink_to('../aclocal')
config = (root/'share/autoconf/autom4te.cfg').read_text()
Path('/workspace/TOOLS/AUTOM4.CFG').write_text(config.replace(
    '/usr/share/autoconf', str(root/'share/autoconf')))
PY
python3 -m venv /workspace/TOOLS
/workspace/TOOLS/bin/python -m pip install --no-cache-dir --disable-pip-version-check \
  py7zr==1.1.4 pefile==2024.8.26
sh BUILD.SH
sh ATTEST.SH
sh KERN.SH
sh CRT.SH
sh TLS.SH
sh NET.SH
python3 TESTS/AUDIT.PY
