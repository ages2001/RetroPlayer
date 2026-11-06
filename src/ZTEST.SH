#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
test_out=${RP_TEST_OUT:-BUILD/TEXT}
test_cache=${RP_USE_CACHE:-0}
case "$test_cache" in 0|1) ;; *) echo 'Unsupported cache profile' >&2; exit 2 ;; esac
mkdir -p "$test_out"
python3 SCRIPTS/TXTSAMP.PY
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-builtin -fno-stack-protector -nostdlib \
  -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -DRP_CODEC_THREADS=2 -DRP_WITH_AV1=0 \
  -DRP_USE_CACHE="$test_cache" \
  -DRP_VIDEO_FILES=2 -DRP_RGB_TEST=1 '-DRP_MEDIA_HEADER="ZDATA.H"' \
  '-DRP_REPORT_NAME="ZTEST.TXT"' '-DRP_LOG_NAME="ZLOG.TXT"' \
  -IBUILD -I/workspace/TXTBLD/include -I/workspace/VLCWORK/include \
  -o "$test_out/ZTEST.EXE" TESTS/MEDTEST.C \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
cp BUILD/SAMPLES/PNG.AVI BUILD/SAMPLES/ZMBV.AVI BUILD/SAMPLES/SUBS.MP4 \
  BUILD/SAMPLES/SUBS.SRT BUILD/SAMPLES/SUBS.ASS "$test_out/"
