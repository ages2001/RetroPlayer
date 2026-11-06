#!/bin/sh
# Update the core and native modules without rebuilding codec dependencies.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
# RP_XMOD_OUT redirects packaging so a rebuild can preserve BUILD/ASM.
xmod_out=${RP_XMOD_OUT:-BUILD/ASM}
make -C /workspace/ASMBLD/src -j"${RP_JOBS:-4}"
mkdir -p "$xmod_out"
cp /workspace/ASMBLD/src/.libs/libcore.dll "$xmod_out/LIBCORE.DLL"
i686-w64-mingw32-strip --strip-unneeded "$xmod_out/LIBCORE.DLL"
make -C /workspace/ASMBLD/modules -j"${RP_JOBS:-4}" \
 libogg_plugin.la libopus_plugin.la libvorbis_plugin.la libflac_plugin.la libavformat_plugin.la \
 libts_plugin.la libasf_plugin.la libwaveout_plugin.la libugly_resampler_plugin.la \
 libspeex_plugin.la libspeex_resampler_plugin.la \
 'am_libts_plugin_la_rpath=-rpath /workspace/RPLIB/lib/vlc/plugins/demux' \
 'am_libspeex_resampler_plugin_la_rpath=-rpath /workspace/RPLIB/lib/vlc/plugins/audio_filter' \
 DVBPSI_CFLAGS=-I/workspace/ASMLIB/include DVBPSI_LIBS=/workspace/ASMLIB/lib/libdvbpsi.a \
 OGG_CFLAGS=-I/workspace/ASMLIB/include OGG_LIBS=/workspace/ASMLIB/lib/libogg.a \
 LIBVORBIS_CFLAGS='-I/workspace/ASMLIB/include -DHAVE_LIBVORBIS=1' \
 LIBVORBIS_LIBS='/workspace/ASMLIB/lib/libvorbis.a /workspace/ASMLIB/lib/libogg.a' \
 OPUS_CFLAGS='-I/workspace/ASMLIB/include/opus -DOPUS_HAVE_OPUS_PROJECTION_H=1' \
 OPUS_LIBS=/workspace/ASMLIB/lib/libopus.a \
 VORBIS_CFLAGS=-I/workspace/ASMLIB/include \
 VORBIS_LIBS='/workspace/ASMLIB/lib/libvorbisenc.a /workspace/ASMLIB/lib/libvorbis.a /workspace/ASMLIB/lib/libogg.a' \
 FLAC_CFLAGS='-I/workspace/ASMLIB/include -DFLAC__NO_DLL' \
 FLAC_LIBS='/workspace/ASMLIB/lib/libFLAC.a /workspace/ASMLIB/lib/libogg.a' \
 SPEEX_CFLAGS=-I/workspace/ASMLIB/include \
 SPEEX_LIBS='/workspace/ASMLIB/lib/libspeex.a /workspace/ASMLIB/lib/libogg.a' \
 SPEEXDSP_CFLAGS=-I/workspace/ASMLIB/include \
 SPEEXDSP_LIBS=/workspace/ASMLIB/lib/libspeexdsp.a
test -f /workspace/ASMBLD/modules/.libs/libts_plugin.dll
xmod_out=$xmod_out python3 - <<'PY'
from pathlib import Path
import os
import subprocess
source=Path('/workspace/ASMBLD/modules/.libs')
names=sorted(p.name[3:-11] for p in source.glob('lib*_plugin.dll'))
subprocess.run(['python3','SCRIPTS/PLUGINS.PY',str(source),os.environ['xmod_out']+'/PLUGINS']+names,check=True)
PY
i686-w64-mingw32-strip --strip-unneeded "$xmod_out"/PLUGINS/*.DLL
python3 SCRIPTS/AUDIT.PY "$xmod_out"
