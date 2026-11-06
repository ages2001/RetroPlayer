#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
export PKG_CONFIG_LIBDIR=/workspace/THRLIB/lib/pkgconfig
mkdir -p BUILD/DEPS
./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror -march=i386 \
  -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll \
  -D_WIN32_WINNT=0x0600 -DRP_LEGACY=1 -DLIBXML_STATIC -DFRIBIDI_LIB_STATIC \
  $(pkg-config --cflags zlib libxml-2.0 freetype2 harfbuzz fribidi) \
  -c TESTS/DEPTST.C -o BUILD/DEPTST.O
./GPP386.SH -mcrtdll=crtdll -static-libgcc -static-libstdc++ -o BUILD/DEPS/DEPTST.EXE \
  BUILD/DEPTST.O /workspace/THRLIB/lib/libiconv.a \
  -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
  $(pkg-config --libs --static libxml-2.0 harfbuzz fribidi freetype2 zlib) \
  -Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0 \
  -Wl,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp
cp /workspace/RPLIB/bin/*.DLL BUILD/DEPS/
cp SOURCES/DEJAVU.TTF BUILD/DEPS/
i686-w64-mingw32-strip --strip-unneeded BUILD/DEPS/*.EXE BUILD/DEPS/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/DEPS
