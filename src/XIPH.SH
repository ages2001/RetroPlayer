#!/bin/sh
# Portable i386 Xiph libraries for VLC's native audio and Ogg modules.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh XPREP.SH
export CC="$PWD/GCC386.SH"
export CFLAGS='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll'
export CPPFLAGS='-I/workspace/ASMLIB/include'
export LDFLAGS='-L/workspace/ASMLIB/lib'
export LIBS='-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive'
export PKG_CONFIG_LIBDIR=/workspace/ASMLIB/lib/pkgconfig
for name in OG VOR FL OP; do
 mkdir -p "/workspace/${name}BLD"
 case "$name" in
  OG) flags='--disable-docs' ;;
  VOR) flags='--disable-docs --disable-examples' ;;
  FL) flags='--disable-cpplibs --disable-programs --disable-examples --disable-doxygen-docs --disable-asm-optimizations --disable-avx' ;;
  OP) flags='--disable-extra-programs --disable-doc --disable-intrinsics --disable-asm --disable-rtcd' ;;
 esac
 (
  cd "/workspace/${name}BLD"
  "/workspace/${name}WORK/configure" --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
   --prefix=/workspace/ASMLIB --disable-shared --enable-static $flags
  make -j"${RP_JOBS:-4}"
  make install
 )
done
