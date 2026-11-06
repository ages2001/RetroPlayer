#!/bin/sh
# Initial portable-C AV1 decoder; assembly dispatch is a separate milestone.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
sh DAVPREP.SH
mkdir -p BUILD
cat > BUILD/DAVCROSS.INI <<'EOF'
[binaries]
c = '/workspace/RETRO/GCC386.SH'
ar = 'i686-w64-mingw32-ar'
strip = 'i686-w64-mingw32-strip'
windres = 'i686-w64-mingw32-windres'
pkg-config = 'pkg-config'
[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'i386'
endian = 'little'
[properties]
needs_exe_wrapper = true
[built-in options]
c_args = ['-O2', '-march=i386', '-mno-sse', '-mno-sse2', '-mno-mmx', '-mstackrealign', '-fno-inline-atomics', '-mcrtdll=crtdll', '-DRP_LEGACY=1']
c_link_args = ['-mcrtdll=crtdll', '-Wl,--whole-archive,/workspace/RPLIB/lib/atom.a,/workspace/RPLIB/lib/kern.a,/workspace/RPLIB/lib/crt.a,--no-whole-archive']
EOF
meson setup /workspace/DAVBLD /workspace/DAVWORK --cross-file "$PWD/BUILD/DAVCROSS.INI" \
  --prefix=/workspace/THRLIB --libdir=lib --default-library=static \
  -Denable_asm=false -Denable_tools=false -Denable_tests=false -Dlegacy_windows=true
ninja -C /workspace/DAVBLD -j2
meson install -C /workspace/DAVBLD
