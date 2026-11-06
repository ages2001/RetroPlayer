#!/bin/sh
# Package only the diagnostic core for private VM loading/thread tests.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
core_dir=${RP_CORE_DIR:-/workspace/CRTBLD}
core_out=${RP_CORE_OUT:-BUILD/ENGINE}
core_media=${RP_CORE_MEDIA:-0}
core_threads=${RP_CORE_THREADS:-1}
core_av1=${RP_CORE_AV1:-0}
core_cache=${RP_USE_CACHE:-0}
case "$core_cache" in 0|1) ;; *) echo 'Unsupported cache profile' >&2; exit 2 ;; esac
case "$core_av1" in 0|1) ;; *) echo 'Unsupported AV1 profile' >&2; exit 2 ;; esac
case "$core_threads" in 1|2|4|8) ;; *) echo 'Unsupported test thread count' >&2; exit 2 ;; esac
mkdir -p "$core_out"
programs='CORETEST PCMTEST STAGE CORETRY'
if [ "$core_media" = 1 ]; then
  python3 SCRIPTS/SAMPLES.PY
  programs="$programs MEDTEST MEDTRY"
fi
for program in $programs; do
  source="TESTS/$program.C"
  extra=''
  if [ "$program" = CORETRY ]; then source=TESTS/SUITE.C; extra=-DCORE_ONLY; fi
  if [ "$program" = MEDTRY ]; then source=TESTS/SUITE.C; extra=-DMEDIA_ONLY; fi
  i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
    -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
    -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
    -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 \
    -DRP_CODEC_THREADS="$core_threads" -DRP_WITH_AV1="$core_av1" -DRP_USE_CACHE="$core_cache" \
    -IBUILD -I"$core_dir/include" -I/workspace/VLCWORK/include \
    $extra -c "$source" -o "BUILD/$program.O"
  i686-w64-mingw32-gcc -nostdlib -o "$core_out/$program.EXE" "BUILD/$program.O" \
    -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
    -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
    -Wl,--no-insert-timestamp -lkernel32 -luser32
done
cp "$core_dir/src/.libs/libcore.dll" "$core_out/LIBCORE.DLL"
cp "$core_dir/lib/.libs/libvlc.dll" "$core_out/LIBVLC.DLL"
cp /workspace/RPLIB/bin/*.DLL "$core_out/"
cp DIST/RETRO.EXE "$core_out/"
mkdir -p "$core_out/PLUGINS"
extra_modules=''
if [ "$core_media" = 1 ]; then extra_modules='avcodec avformat swscale vmem rpgdi waveout'; fi
if [ "$core_av1" = 1 ]; then extra_modules="$extra_modules dav1d"; fi
if [ "$core_media" = 1 ]; then
  cp BUILD/SAMPLES/*.MP4 BUILD/SAMPLES/*.MKV BUILD/SAMPLES/*.WEB BUILD/SAMPLES/*.M4A "$core_out/"
  cp BUILD/KTEST/KTEST.EXE BUILD/CRTEST/CRTEST.EXE "$core_out/"
  cp BUILD/SYNC/SYNCTEST.EXE "$core_out/"
fi
python3 SCRIPTS/PLUGINS.PY "$core_dir/modules/.libs" "$core_out/PLUGINS" \
  dummy amem araw wav filesystem integer_mixer float_mixer audio_format \
  trivial_channel_mixer ugly_resampler $extra_modules
i686-w64-mingw32-strip --strip-unneeded "$core_out/"*.DLL
i686-w64-mingw32-strip --strip-unneeded "$core_out/PLUGINS/"*.DLL
python3 SCRIPTS/AUDIT.PY "$core_out"
