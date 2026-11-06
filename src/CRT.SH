#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD /workspace/RPLIB/bin /workspace/RPLIB/lib
cat > BUILD/CRT.DEF <<'EOF'
LIBRARY RPCRT.DLL
EXPORTS
 _time32=rp_time
 _mktime32=rp_mktime
 _gmtime32_s=rp_gmtime
 _localtime32_s=rp_localtime
 fputws=rp_fputws
 _wmkdir=rp_wmkdir
 _wunlink=rp_wunlink
 _wremove=rp_wremove
 _wrename=rp_wrename
 _wgetcwd=rp_wgetcwd
 _wfullpath=rp_wfullpath
 _wfindfirst32=rp_wfindfirst
 _wfindnext32=rp_wfindnext
 _findfirst32=rp_findfirst
 _findnext32=rp_findnext
 _findclose=rp_findclose
 _fstati64=rp_fstati64
 _stati64=rp_stati64
 _wstati64=rp_wstati64
 _lseeki64=rp_lseeki64
 __emutls_get_address=rp_tls_address
 __emutls_register_common=rp_tls_register
 _aligned_malloc=rp_aligned_malloc
 _aligned_realloc=rp_aligned_realloc
 _aligned_free=rp_aligned_free
 _wopen=rp_wopen
 _wfopen=rp_wfopen
 _wstat32=rp_wstat32
 _wsopen=rp_wsopen
 _waccess=rp_waccess
 _wchmod=rp_wchmod
 _wutime64=rp_wutime64
 _filelengthi64=rp_filelengthi64
 _wrmdir=rp_wrmdir
 _stat64=rp_stat64
 _wstat64=rp_wstat64
 _fstat64=rp_fstat64
 _wgetenv=rp_wgetenv
 rand_s=rp_rand_s
 RPSetJmp=rp_setjmp
 RPLongJmp=rp_longjmp
EOF
python3 - <<'PY'
from pathlib import Path
names={'create':'create','join':'join','self':'self','detach':'detach','equal':'equal',
 'yield':'yield','once':'once','once_reset':'once_reset','key_create':'key_create','key_delete':'key_delete',
 'once_begin':'once_begin','once_complete':'once_complete',
 'getspecific':'getspecific','setspecific':'setspecific',
 'mutex_init_function':'mutex_init','mutex_destroy':'mutex_destroy',
 'mutex_lock':'mutex_lock','mutex_unlock':'mutex_unlock','mutex_trylock':'mutex_try',
 'recursive_mutex_trylock':'mutex_try','cond_init_function':'cond_init',
 'cond_signal':'cond_signal','cond_broadcast':'cond_broadcast',
 'cond_wait':'cond_wait','cond_timedwait':'cond_timedwait'}
with Path('BUILD/CRT.DEF').open('a') as f:
 for public,private in names.items(): f.write(f' __gthr_win32_{public}=rp_gthr_{private}\n')
PY
for unit in CRT CRTFILE TLS ALIGN ENV RAND GTHREAD; do
api_level=0x0502
if [ "$unit" = GTHREAD ]; then api_level=0x0600; fi
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -mcrtdll=crtdll \
  -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT="$api_level" -DWINVER="$api_level" -c "SRC/$unit.C" -o "BUILD/$unit.O"
done
i686-w64-mingw32-gcc -x assembler-with-cpp -march=i386 -c SRC/JUMP.S -o BUILD/JUMP.O
i686-w64-mingw32-gcc -shared -nostdlib -mcrtdll=crtdll \
  -o /workspace/RPLIB/bin/RPCRT.DLL BUILD/CRT.O BUILD/CRTFILE.O BUILD/TLS.O BUILD/ALIGN.O BUILD/ENV.O BUILD/RAND.O BUILD/GTHREAD.O BUILD/JUMP.O BUILD/CRT.DEF \
  -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp,--out-implib,/workspace/RPLIB/lib/crt.a \
  /workspace/RPLIB/lib/kern.a -lcrtdll -lkernel32 -lgcc
python3 SCRIPTS/AUDIT.PY /workspace/RPLIB/bin
mkdir -p BUILD/CRTEST
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -mcrtdll=crtdll \
  -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -c TESTS/CRTEST.C -o BUILD/CRTEST.O
i686-w64-mingw32-gcc -nostdlib -mcrtdll=crtdll -o BUILD/CRTEST/CRTEST.EXE BUILD/CRTEST.O \
  /workspace/RPLIB/lib/crt.a /workspace/RPLIB/lib/kern.a \
  -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lcrtdll -lkernel32 -luser32
cp /workspace/RPLIB/bin/RPCRT.DLL /workspace/RPLIB/bin/RPKERN.DLL BUILD/CRTEST/
python3 SCRIPTS/AUDIT.PY BUILD/CRTEST
