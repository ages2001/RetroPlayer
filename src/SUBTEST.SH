#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
test_out=${RP_TEST_OUT:-BUILD/TEXT}
test_cache=${RP_USE_CACHE:-0}
case "$test_cache" in 0|1) ;; *) echo 'Unsupported cache profile' >&2; exit 2 ;; esac
mkdir -p "$test_out"
for test in SUBTEST PCMTEST ASSVID; do
 source="TESTS/$test.C"
 extra=''
 if [ "$test" = ASSVID ]; then
  source=TESTS/SUBTEST.C
  extra='-DRP_ASS_TEST=1 -DRP_SUB_FILE="SUBS.ASS" -DRP_REPORT_NAME="ASSVID.TXT" -DRP_LOG_NAME="ASSLOG.TXT" -DRP_CAPTURE_NAME="ASS.PPM"'
 fi
 ./GCC386.SH -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx \
  -fno-builtin -fno-stack-protector -mcrtdll=crtdll \
  -DRP_USE_CACHE="$test_cache" \
  -I/workspace/TXTBLD/include -I/workspace/VLCWORK/include \
  $extra -c "$source" -o "BUILD/$test.O"
 extra_link='-nostdlib -Wl,--entry,_RPEntry@0'
 if [ "$test" != PCMTEST ]; then extra_link='-static-libgcc'; fi
 ./GCC386.SH $extra_link -mcrtdll=crtdll -o "$test_out/$test.EXE" "BUILD/$test.O" \
  -Wl,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp \
  -Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive \
  -lmingwex -lcrtdll -lkernel32 -luser32 -lgcc
done
suite_flag=-DTEXT_ONLY
if [ "$test_cache" = 1 ]; then suite_flag=-DASM_ONLY; fi
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin -fno-stack-protector \
 -nostdlib "$suite_flag" -o "$test_out/TXTTRY.EXE" TESTS/SUITE.C \
 -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
 -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
 -Wl,--no-insert-timestamp -lkernel32 -luser32
