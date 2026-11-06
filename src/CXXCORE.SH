#!/bin/sh
# Build the i386 C++ runtime. Run CXXPREP.SH and MWCRT.SH first.
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
export RP_BUILD_CXX=1
mkdir -p /workspace/CXX600
cd /workspace/CXX600
rp_flags='-O2 -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-inline-atomics -mcrtdll=crtdll -D_WIN32_WINNT=0x0600 -DRP_LEGACY=1'
# Start from consistent generated headers after changing runtime options.
if [ -f Makefile ]; then make clean; fi
/workspace/GCCWORK/libstdc++-v3/configure --host=i686-w64-mingw32 \
  --build=x86_64-pc-linux-gnu --target=i686-w64-mingw32 --prefix=/workspace/CXXLIB \
  --disable-multilib --disable-shared --enable-static --disable-nls \
  --disable-libstdcxx-pch --disable-libstdcxx-verbose --disable-libstdcxx-backtrace \
  --disable-libstdcxx-filesystem-ts --with-libstdcxx-zoneinfo=no \
  --enable-libstdcxx-threads --with-gxx-include-dir=/workspace/CXXLIB/include/c++/14 \
  CC=/workspace/RETRO/GCC386.SH CXX=/workspace/RETRO/GPP386.SH \
  CFLAGS="$rp_flags" CXXFLAGS="$rp_flags"
make -j"${RP_JOBS:-4}"
make install
cd /workspace/RETRO
sh UNWIND.SH
