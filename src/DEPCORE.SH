#!/bin/sh
# Portable static dependencies for compressed media and text rendering.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh DEPPREP.SH
sh ICONV.SH
export PKG_CONFIG_LIBDIR=/workspace/THRLIB/lib/pkgconfig
dep_prefix=/workspace/THRLIB
dep_flags='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DRP_LEGACY=1'
mkdir -p "$dep_prefix/lib/pkgconfig" "$dep_prefix/include"
make -C /workspace/ZWORK -f win32/Makefile.gcc -j"${RP_JOBS:-4}" libz.a \
  PREFIX=i686-w64-mingw32- CC="$PWD/GCC386.SH" CFLAGS="$dep_flags"
cp /workspace/ZWORK/libz.a "$dep_prefix/lib/"
cp /workspace/ZWORK/zlib.h /workspace/ZWORK/zconf.h "$dep_prefix/include/"
sed -e "s|@prefix@|$dep_prefix|g" -e 's|@exec_prefix@|${prefix}|g' \
  -e 's|@libdir@|${prefix}/lib|g' -e 's|@sharedlibdir@|${prefix}/lib|g' \
  -e 's|@includedir@|${prefix}/include|g' -e 's|@VERSION@|1.3.2|g' \
  /workspace/ZWORK/zlib.pc.in > "$dep_prefix/lib/pkgconfig/zlib.pc"
python3 - <<'PY'
from pathlib import Path
args=['-O2','-march=i386','-mno-sse','-mno-sse2','-mno-mmx','-mstackrealign',
      '-fno-inline-atomics','-mcrtdll=crtdll','-DRP_LEGACY=1',
      '-D_WIN32_WINNT=0x0600','-DWINVER=0x0600']
links=['-mcrtdll=crtdll','-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive']
p="""[binaries]
c = '/workspace/RETRO/GCC386.SH'
cpp = '/workspace/RETRO/GPP386.SH'
ar = 'i686-w64-mingw32-ar'
strip = 'i686-w64-mingw32-strip'
windres = 'i686-w64-mingw32-windres'
pkg-config = 'pkg-config'
[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'i386'
endian = 'little'
[properties]
needs_exe_wrapper = true
[built-in options]
"""
for lang in ('c','cpp'):
    p+=lang+'_args = '+repr(args)+'\n'+lang+'_link_args = '+repr(links)+'\n'
Path('BUILD/DEPCROSS.INI').write_text(p)
PY
dep_cross="$PWD/BUILD/DEPCROSS.INI"
setup_dep() {
  dep_name=$1; dep_src=$2; shift 2
  meson setup --reconfigure "/workspace/${dep_name}BLD" "$dep_src" \
    --cross-file "$dep_cross" --prefix="$dep_prefix" --libdir=lib \
    --default-library=static --wrap-mode=nofallback "$@"
  if [ "$dep_name" = XMLNEW ]; then
    ninja -C "/workspace/${dep_name}BLD" -j"${RP_JOBS:-4}" libxml2.a
    meson install -C "/workspace/${dep_name}BLD" --no-rebuild --tags devel
  else
    ninja -C "/workspace/${dep_name}BLD" -j"${RP_JOBS:-4}"
    meson install -C "/workspace/${dep_name}BLD"
  fi
}
setup_dep FTNEW /workspace/FTWORK -Dbrotli=disabled -Dbzip2=disabled \
  -Dharfbuzz=disabled -Dpng=disabled -Dzlib=system -Dmmap=disabled
setup_dep FR /workspace/FRWORK -Ddocs=false -Dbin=false -Dtests=false
setup_dep HB /workspace/HBWORK -Dauto_features=disabled -Dfreetype=enabled \
  -Dglib=disabled -Dtests=disabled -Dutilities=disabled -Draster=disabled \
  -Dvector=disabled -Dgpu=disabled -Dsubset=disabled
setup_dep XMLNEW /workspace/XMLWORK -Ddocs=disabled -Dpython=disabled \
  -Diconv=disabled -Dicu=disabled -Dreadline=disabled -Dhistory=disabled \
  -Dtls=disabled -Dmodules=disabled -Dzlib=enabled -Dlegacy_windows=true
sh ASSCORE.SH
