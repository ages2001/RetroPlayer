#!/bin/sh
# Native MPEG transport streams with VLC's pinned DVBPSI fixes.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
python3 - <<'PY'
import hashlib,json,subprocess
from pathlib import Path
item=json.loads(Path('SOURCES/DVBPSI.JSN').read_text())
archive=Path('SOURCES')/item['archive']
if hashlib.sha256(archive.read_bytes()).hexdigest()!=item['sha256']:
    raise SystemExit('DVBPSI source checksum failed')
work=Path('/workspace/DVWORK')
if not work.exists():
    work.mkdir()
    subprocess.run(['tar','xf',str(archive),'-C',str(work),'--strip-components=1'],check=True)
PY
if patch --batch --forward --dry-run -p1 -d /workspace/DVWORK < PATCHES/DVBPSI.DIF >/dev/null 2>&1; then
 patch --batch --forward -p1 -d /workspace/DVWORK < PATCHES/DVBPSI.DIF
else
 patch --batch --reverse --dry-run -p1 -d /workspace/DVWORK < PATCHES/DVBPSI.DIF >/dev/null
fi
cp /workspace/DVWORK/COPYING LICENSES/DVBPSI.TXT
(cd /workspace/DVWORK && autoreconf -isf)
mkdir -p /workspace/DVBLD
(
 cd /workspace/DVBLD
 CC=/workspace/RETRO/GCC386.SH \
 CFLAGS='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll' \
 LIBS='-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive' \
 /workspace/DVWORK/configure --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
 --prefix=/workspace/ASMLIB --enable-static --disable-shared
 make -j"${RP_JOBS:-4}"
 make install
)
# This target was disabled at initial core configuration, so supply its
# installation path explicitly; otherwise libtool creates only a static archive.
make -C /workspace/ASMBLD/modules -W demux/mpeg/libts_plugin_la-ts.lo -j"${RP_JOBS:-4}" \
 libts_plugin.la libasf_plugin.la libwaveout_plugin.la \
 'am_libts_plugin_la_rpath=-rpath /workspace/RPLIB/lib/vlc/plugins/demux' \
 DVBPSI_CFLAGS=-I/workspace/ASMLIB/include \
 DVBPSI_LIBS=/workspace/ASMLIB/lib/libdvbpsi.a
sh XMOD.SH
