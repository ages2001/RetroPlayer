#!/bin/sh
# Build portable codec and resampler libraries; module integration is separate.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh SPPREP.SH
export CC="$PWD/GCC386.SH"
export CFLAGS='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll'
export CPPFLAGS='-I/workspace/ASMLIB/include'
export LDFLAGS='-L/workspace/ASMLIB/lib'
export LIBS='-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive'
export PKG_CONFIG_LIBDIR=/workspace/ASMLIB/lib/pkgconfig
for name in SP SD; do
 mkdir -p "/workspace/${name}BLD"
 case "$name" in
  SP) flags='--disable-binaries --disable-sse' ;;
  SD) flags='--disable-examples --disable-neon --disable-sse --enable-resample-full-sinc-table' ;;
 esac
 (
  cd "/workspace/${name}BLD"
  "/workspace/${name}WORK/configure" --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
   --prefix=/workspace/ASMLIB --disable-shared --enable-static $flags
  make -j"${RP_JOBS:-4}"
  make install
 )
done
cp /workspace/SPWORK/COPYING LICENSES/SPEEX.TXT
cp /workspace/SDWORK/COPYING LICENSES/SPEEXDSP.TXT
