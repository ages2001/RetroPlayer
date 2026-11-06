#!/bin/sh
# Portable libass renderer; platform font discovery is provided by VLC.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh DEPPREP.SH
ass_prefix=${RP_CODEC_PREFIX:-/workspace/THRLIB}
export PKG_CONFIG_LIBDIR="$ass_prefix/lib/pkgconfig"
mkdir -p /workspace/ASSBLD
cd /workspace/ASSBLD
/workspace/ASSWORK/configure --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
 --prefix="$ass_prefix" --enable-static --disable-shared --disable-asm \
 --disable-fontconfig --disable-directwrite --disable-coretext \
 --disable-require-system-font-provider --disable-test \
 CC=/workspace/RETRO/GCC386.SH \
 CFLAGS='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -DRP_LEGACY=1' \
 CPPFLAGS="-I$ass_prefix/include -DFRIBIDI_LIB_STATIC" \
 LDFLAGS="-L$ass_prefix/lib" \
 LIBS='-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive'
make -j"${RP_JOBS:-4}"
make install
cp /workspace/ASSWORK/COPYING /workspace/RETRO/LICENSES/ASS.TXT
