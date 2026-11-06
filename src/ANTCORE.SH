#!/bin/sh
# Build the user-requested AUDIONT legacy NT driver from the pinned checkout.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
ant_source=/workspace/AUDIONT
ant_stage=/workspace/ANTDIST
test "$(git -C "$ant_source" rev-parse HEAD)" = ff36dc8e14f918c62f73761a774a5fc9688583bf
make -C "$ant_source" -j4 all tools PY=/workspace/TOOLS/bin/python
# Also verify an existing target left behind by an interrupted post-link step.
/workspace/TOOLS/bin/python "$ant_source/tools/pefix.py" "$ant_source/build/audiont.sys" driver
mkdir -p "$ant_stage/FW"
cp "$ant_source/build/audiont.sys" "$ant_stage/AUDIONT.SYS"
cp "$ant_source/build/audiont.dll" "$ant_stage/AUDIONT.DLL"
cp "$ant_source/build/setup.exe" "$ant_stage/SETUP.EXE"
cp "$ant_source/build/audmix.exe" "$ant_stage/AUDMIX.EXE"
cp "$ant_source/build/attest.exe" "$ant_stage/AUDTEST.EXE"
cp "$ant_source/tools/README.TXT" "$ant_stage/README.TXT"
cp "$ant_source/LICENSE" "$ant_stage/LICENSE.TXT"
python3 - <<'PY'
from pathlib import Path
import shutil
for p in Path('/workspace/AUDIONT/fw').iterdir():
    if p.is_file():shutil.copyfile(p,Path('/workspace/ANTDIST/FW')/p.name.upper())
PY
