#!/bin/sh
# Diagnostic software decoder build, not the final feature/acceleration profile.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh FFPREP.SH
ff_dir=${RP_FF_DIR:-/workspace/FFBLD}
ff_prefix=${RP_FF_PREFIX:-/workspace/RPLIB}
ff_threads=${RP_FF_THREADS:-0}
ff_asm=${RP_FF_ASM:-0}
case "$ff_asm" in
  0) asm_flags='--disable-asm' ;;
  1) asm_flags='--enable-asm --enable-x86asm' ;;
  *) echo 'Unsupported assembly profile' >&2; exit 2 ;;
esac
case "$ff_threads" in
  0) thread_flag=--disable-w32threads; api_level=0x0502 ;;
  1) thread_flag=--enable-w32threads; api_level=0x0600 ;;
  *) echo 'Unsupported thread profile' >&2; exit 2 ;;
esac
mkdir -p "$ff_dir"
export PKG_CONFIG_LIBDIR="$ff_prefix/lib/pkgconfig"
ff_compression='--disable-zlib'
ff_dep_cflags=''
ff_dep_ldflags=''
if [ -f "$ff_prefix/lib/libz.a" ]; then
  ff_compression='--enable-zlib'
  ff_dep_cflags="-I$ff_prefix/include"
  ff_dep_ldflags="-L$ff_prefix/lib"
fi
cd "$ff_dir"
/workspace/FFWORK/configure --target-os=mingw32 --arch=x86 --cpu=i386 \
  --enable-cross-compile --cross-prefix=i686-w64-mingw32- \
  --cc=/workspace/RETRO/GCC386.SH --cxx=/workspace/RETRO/GPP386.SH \
  --prefix="$ff_prefix" --disable-programs --disable-doc --disable-debug \
  --disable-shared --enable-static --disable-avdevice --disable-avfilter \
  --disable-encoders --disable-muxers --disable-pthreads "$thread_flag" \
  $asm_flags --disable-autodetect "$ff_compression" \
  --extra-cflags="-O2 -march=i386 -mno-mmx -mno-sse -mno-sse2 -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=$api_level -DWINVER=$api_level -DRP_LEGACY=1 $ff_dep_cflags" \
  --extra-ldflags="-mcrtdll=crtdll $ff_dep_ldflags" \
  --extra-libs='-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,/workspace/RPLIB/lib/net.a,--no-whole-archive'
make -j"${RP_JOBS:-4}"
make install
