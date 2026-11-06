#!/bin/sh
# Diagnostic VLC core build. Disabled modules here are NOT a release profile.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh ATOM.SH
sh SYNC.SH
sh CRT.SH
sh NET.SH
core_dir=${RP_CORE_DIR:-/workspace/CRTBLD}
core_crt=${RP_CORE_CRT:-crtdll}
core_media=${RP_CORE_MEDIA:-0}
core_av1=${RP_CORE_AV1:-0}
core_all=${RP_CORE_ALL:-0}
core_text=${RP_CORE_TEXT:-0}
case "$core_text" in
  0) text_flags='--disable-freetype' ;;
  1) text_flags="--enable-freetype --enable-fribidi --enable-harfbuzz --enable-libxml2 --enable-libass --with-libiconv-prefix=${RP_CODEC_PREFIX:-/workspace/RPLIB}" ;;
  *) echo 'Unsupported text profile' >&2; exit 2 ;;
esac
case "$core_all" in 0|1) ;; *) echo 'Unsupported module profile' >&2; exit 2 ;; esac
case "$core_av1" in
  0) av1_flag=--disable-dav1d ;;
  1) av1_flag=--enable-dav1d ;;
  *) echo 'Unsupported AV1 profile' >&2; exit 2 ;;
esac
case "$core_media" in
  0) media_flags='--disable-avcodec --disable-avformat --disable-swscale' ;;
  1) media_flags='--enable-avcodec --enable-avformat --enable-swscale --disable-dxva2 --disable-d3d11va' ;;
  *) echo 'Unsupported diagnostic media profile' >&2; exit 2 ;;
esac
case "$core_crt" in msvcrt|crtdll) ;; *) echo 'Unsupported CRT' >&2; exit 2 ;; esac
mkdir -p "$core_dir" /workspace/RPLIB/lib/pkgconfig
cd "$core_dir"
export PKG_CONFIG_LIBDIR=${RP_CODEC_PREFIX:-/workspace/RPLIB}/lib/pkgconfig
core_flags='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -D_WIN32_WINNT=0x0502 -DWINVER=0x0502'
core_flags="$core_flags -mcrtdll=$core_crt -DRP_LEGACY=1"
if [ "$core_text" = 1 ]; then core_flags="$core_flags -DLIBXML_STATIC -DFRIBIDI_LIB_STATIC"; fi
core_cxx_flags=$(printf '%s' "$core_flags" | sed 's/0x0502/0x0600/g')
/workspace/VLCWORK/configure --host=i686-w64-mingw32 --build=x86_64-pc-linux-gnu \
  --prefix=/workspace/RPLIB --enable-retro --disable-optimizations \
  --disable-vlc --disable-nls --disable-lua \
  --disable-qt --disable-skins2 $media_flags "$av1_flag" \
  --disable-a52 --disable-mad $text_flags \
  --disable-wasapi \
  --disable-sout --disable-vlm --disable-libgcrypt --disable-update-check \
  --disable-dbus --disable-mmx --disable-sse \
  CC=/workspace/RETRO/GCC386.SH CXX=/workspace/RETRO/GPP386.SH \
  CFLAGS="$core_flags" CXXFLAGS="$core_cxx_flags" \
  LDFLAGS='-Wl,--subsystem,windows:3.10,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat,--no-insert-timestamp' \
  LIBS='-Wl,/workspace/RPLIB/lib/atom.a,--whole-archive,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,/workspace/RPLIB/lib/net.a,--no-whole-archive'
# All objects must agree on the atomic calling convention. Never mix old
# inline atomic stores with the shared fallback's locked read/modify/write.
make -C compat clean
make -C src clean
make -C lib clean
make -j2 -C compat
make -j2 -C src
make -j2 -C lib
make -C modules clean
make -j2 -C modules module.rc.lo
make -j2 -C modules libdummy_plugin.la libamem_plugin.la libaraw_plugin.la \
  libwav_plugin.la libfilesystem_plugin.la libinteger_mixer_plugin.la \
  libfloat_mixer_plugin.la libaudio_format_plugin.la \
  libtrivial_channel_mixer_plugin.la libugly_resampler_plugin.la
if [ "$core_media" = 1 ]; then
  make -j2 -C modules libavcodec_plugin.la libavformat_plugin.la \
    libswscale_plugin.la libvmem_plugin.la librpgdi_plugin.la libwaveout_plugin.la
fi
if [ "$core_av1" = 1 ]; then make -j2 -C modules libdav1d_plugin.la; fi
if [ "$core_all" = 1 ]; then make -j2 -C modules; fi
