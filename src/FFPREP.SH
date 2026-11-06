#!/bin/sh
set -eu
cd "$(dirname "$0")"
revision=38b88335f99e76ed89ff3c93f877fdefce736c13
if [ ! -d /workspace/FFREF/.git ]; then
  git clone --depth 1 --branch n8.1.2 https://github.com/FFmpeg/FFmpeg.git /workspace/FFREF
fi
test "$(git -C /workspace/FFREF rev-parse HEAD)" = "$revision"
test -z "$(git -C /workspace/FFREF status --porcelain)"
if [ ! -d /workspace/FFWORK ]; then
  git -C /workspace/FFREF archive --format=tar -o /workspace/TOOLS/FFBASE.TAR "$revision"
  mkdir /workspace/FFWORK
  tar -xf /workspace/TOOLS/FFBASE.TAR -C /workspace/FFWORK
fi
patch_file="$PWD/PATCHES/FFMPEG.DIF"
if patch --batch --forward --dry-run -p1 -d /workspace/FFWORK < "$patch_file" >/dev/null 2>&1; then
  patch --batch --forward -p1 -d /workspace/FFWORK < "$patch_file"
elif ! patch --batch --reverse --dry-run -p1 -d /workspace/FFWORK < "$patch_file" >/dev/null 2>&1; then
  echo 'Unexpected FFmpeg working changes; preserving them for review' >&2
  exit 2
fi
