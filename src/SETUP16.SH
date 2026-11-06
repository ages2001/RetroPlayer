#!/bin/sh
# Build the 16-bit setup wizard (RPSET16.EXE) and the 16-bit uninstaller
# (RPUN16.EXE, red-X icon). Open Watcom v2, 8086 code, large model, so they run
# on Windows 3.0 and 3.1. SCRIPTS/INSPK16.PY then appends the player payload.
set -eu
cd "$(dirname "$0")"
: "${WATCOM:=/workspace/TOOLS/OW}"
export WATCOM PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h:$WATCOM/h/win"
mkdir -p BUILD/W16 DIST16

# --- setup wizard stub -----------------------------------------------------
wcc -q -bt=windows -ml -0 -zW -zu -s -os -wx -we -zq \
  -fo=BUILD/W16/RPSET16.OBJ SRC16/RPSET16.C
cat > BUILD/W16/RPSET16.LNK <<'LNK'
system windows
name BUILD/W16/RPSET16.EXE
option quiet
option description 'RetroPlayer 16 Setup'
option heapsize=8192
option stack=16384
file BUILD/W16/RPSET16.OBJ
LNK
wlink @BUILD/W16/RPSET16.LNK

# --- uninstaller -----------------------------------------------------------
wcc -q -bt=windows -ml -0 -zW -zu -s -os -wx -we -zq \
  -fo=BUILD/W16/RPUN16.OBJ SRC16/RPUN16.C
wrc -q -r -bt=windows -i=SRC16 -i=SRC -fo=BUILD/W16/RPUN16.RES SRC16/RPUN16.RC
cat > BUILD/W16/RPUN16.LNK <<'LNK'
system windows
name DIST16/RPUN16.EXE
option quiet
option description 'RetroPlayer 16 Uninstall'
option heapsize=8192
option stack=16384
file BUILD/W16/RPUN16.OBJ
option resource=BUILD/W16/RPUN16.RES
LNK
wlink @BUILD/W16/RPUN16.LNK
