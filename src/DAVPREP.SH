#!/bin/sh
set -eu
cd "$(dirname "$0")"
revision=54706fc6bc0cdecab7e9593974a4039cc038fca7
if [ ! -d /workspace/DAVREF/.git ]; then
  git clone --depth 1 --branch 1.5.4 https://github.com/videolan/dav1d.git /workspace/DAVREF
fi
test "$(git -C /workspace/DAVREF rev-parse HEAD)" = "$revision"
test -z "$(git -C /workspace/DAVREF status --porcelain)"
if [ ! -d /workspace/DAVWORK ]; then
  git -C /workspace/DAVREF archive --format=tar -o /workspace/TOOLS/DAVBASE.TAR "$revision"
  mkdir /workspace/DAVWORK
  tar -xf /workspace/TOOLS/DAVBASE.TAR -C /workspace/DAVWORK
fi
patch_file="$PWD/PATCHES/DAV1D.DIF"
if patch --batch --forward --dry-run -p1 -d /workspace/DAVWORK < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 -d /workspace/DAVWORK < "$patch_file"
elif ! patch --batch --reverse --dry-run -p1 -d /workspace/DAVWORK < "$patch_file" >/dev/null 2>&1; then
  echo 'Unexpected dav1d working changes; preserving them for review' >&2
  exit 2
fi
