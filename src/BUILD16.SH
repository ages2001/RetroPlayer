#!/bin/sh
# Build RetroPlayer 16 (RETRO16.EXE), a 16-bit NE Windows 3.0/3.1 program,
# with Open Watcom v2 (see TOOLS16.MD). 8086 instructions only (-0) so the
# program can run in Windows 3.0 real mode; large memory model.
# MMSYSTEM and COMMDLG are loaded at run time only: Windows 3.0 has neither.
set -eu
cd "$(dirname "$0")"
: "${WATCOM:=/workspace/TOOLS/OW}"
export WATCOM PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h:$WATCOM/h/win"
mkdir -p BUILD/W16 DIST16
for unit in RETRO16 DECODE AOUT; do
  wcc -q -bt=windows -ml -0 -zW -zu -s -os -wx -we -zq -fo=BUILD/W16/$unit.OBJ SRC16/$unit.C
done
wrc -q -r -bt=windows -i=SRC16 -fo=BUILD/W16/RETRO16.RES SRC16/RETRO16.RC
# wlink string directives (single-quoted) go in a directive file so the shell
# does not strip their quotes.
cat > BUILD/W16/RETRO16.LNK <<'LNK'
system windows
name DIST16/RETRO16.EXE
option quiet
option description 'RetroPlayer 16'
option heapsize=8192
option stack=12288
file BUILD/W16/RETRO16.OBJ
file BUILD/W16/DECODE.OBJ
file BUILD/W16/AOUT.OBJ
option resource=BUILD/W16/RETRO16.RES
LNK
wlink @BUILD/W16/RETRO16.LNK
