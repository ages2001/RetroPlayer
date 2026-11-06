#!/bin/sh
# Export the pinned source and apply the reproducible RetroPlayer engine patch.
set -eu
cd "$(dirname "$0")"
sh FETCH.SH
. ./BUILDENV.SH
if [ ! -d /workspace/VLCWORK ]; then
  git -C /workspace/VLCREF archive --format=tar \
    -o /workspace/TOOLS/VLCBASE.TAR 6de05adcbaf2e8b85fe86aad4169393098628119
  mkdir /workspace/VLCWORK
  tar -xf /workspace/TOOLS/VLCBASE.TAR -C /workspace/VLCWORK
fi
patch_file="$PWD/PATCHES/LEGACY.DIF"
cd /workspace/VLCWORK
if patch --batch --forward --dry-run -p1 < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 < "$patch_file"
elif patch --batch --reverse --dry-run -p1 < "$patch_file" >/dev/null 2>&1; then
  echo 'RetroPlayer source patch is already applied.'
else
  echo 'Engine source differs from both patch states; preserving it for review.' >&2
  exit 1
fi
printf '%s\n' '3.0.24-0-g6de05ad' > src/revision.txt
./bootstrap
