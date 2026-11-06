#!/bin/sh
set -eu
# Upstream reference is kept outside the 8.3 RetroPlayer distribution.
if [ ! -d /workspace/VLCREF ]; then
  git clone --depth 1 --branch 3.0.24 https://github.com/videolan/vlc.git /workspace/VLCREF
fi
actual=$(git -C /workspace/VLCREF rev-parse HEAD)
expected=6de05adcbaf2e8b85fe86aad4169393098628119
if [ "$actual" != "$expected" ]; then
  echo 'VLCREF contains another revision; preserved without resetting it.' >&2
  exit 1
fi
if [ -n "$(git -C /workspace/VLCREF status --porcelain)" ]; then
  echo 'VLCREF is modified; preserved without resetting it.' >&2
  exit 1
fi
