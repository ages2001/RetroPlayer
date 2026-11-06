#!/bin/sh
# Build candidates separately; validated ASM payload is not changed here.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
# Force the Ogg sources to use the same Vorbis-aware layout as XMOD.SH.
rm -f /workspace/ASMBLD/modules/demux/libogg_plugin_la-ogg.lo \
 /workspace/ASMBLD/modules/demux/libogg_plugin_la-oggseek.lo
make -C /workspace/ASMBLD/modules -j2 libogg_plugin.la libspeex_plugin.la libspeex_resampler_plugin.la \
 OGG_CFLAGS=-I/workspace/ASMLIB/include OGG_LIBS=/workspace/ASMLIB/lib/libogg.a \
 LIBVORBIS_CFLAGS='-I/workspace/ASMLIB/include -DHAVE_LIBVORBIS=1' \
 LIBVORBIS_LIBS='/workspace/ASMLIB/lib/libvorbis.a /workspace/ASMLIB/lib/libogg.a' \
 SPEEX_CFLAGS=-I/workspace/ASMLIB/include \
 SPEEX_LIBS='/workspace/ASMLIB/lib/libspeex.a /workspace/ASMLIB/lib/libogg.a' \
 SPEEXDSP_CFLAGS=-I/workspace/ASMLIB/include \
 SPEEXDSP_LIBS=/workspace/ASMLIB/lib/libspeexdsp.a \
 'am_libspeex_resampler_plugin_la_rpath=-rpath /workspace/RPLIB/lib/vlc/plugins/audio_filter'
python3 SCRIPTS/PLUGINS.PY /workspace/ASMBLD/modules/.libs BUILD/SPMOD ogg speex speex_resampler
i686-w64-mingw32-strip --strip-unneeded BUILD/SPMOD/*.DLL
python3 SCRIPTS/AUDIT.PY BUILD/SPMOD > RESULTS/SPMOD.JSN
