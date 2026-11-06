#!/bin/sh
set -eu
cd "$(dirname "$0")"
. ./BUILDENV.SH
mkdir -p BUILD/KTEST /workspace/RPLIB/bin /workspace/RPLIB/lib
python3 - <<'PY'
from pathlib import Path
functions = [
    ('InitializeCriticalSection', 'rp_cs_init', 4),
    ('EnterCriticalSection', 'rp_cs_enter', 4),
    ('TryEnterCriticalSection', 'rp_cs_try', 4),
    ('LeaveCriticalSection', 'rp_cs_leave', 4),
    ('DeleteCriticalSection', 'rp_cs_delete', 4),
    ('GetNativeSystemInfo', 'rp_system_info', 4),
    ('AreFileApisANSI', 'rp_file_apis_ansi', 0),
    ('SetFileApisToANSI', 'rp_file_apis_ansi_set', 0),
    ('SetFileApisToOEM', 'rp_file_apis_oem', 0),
    ('FindFirstFileW', 'rp_find_first', 8),
    ('FindNextFileW', 'rp_find_next', 8),
    ('FindFirstFileExW', 'rp_find_ex', 24),
    ('FindClose', 'rp_find_close', 4),
    ('GetProcessAffinityMask', 'rp_affinity', 12),
    ('InitializeSRWLock', 'rp_srw_init', 4),
    ('AcquireSRWLockExclusive', 'rp_srw_acquire_exclusive', 4),
    ('AcquireSRWLockShared', 'rp_srw_acquire_shared', 4),
    ('TryAcquireSRWLockExclusive', 'rp_srw_try_exclusive', 4),
    ('TryAcquireSRWLockShared', 'rp_srw_try_shared', 4),
    ('ReleaseSRWLockExclusive', 'rp_srw_release_exclusive', 4),
    ('ReleaseSRWLockShared', 'rp_srw_release_shared', 4),
    ('InitializeConditionVariable', 'rp_condition_init', 4),
    ('WakeConditionVariable', 'rp_condition_wake', 4),
    ('WakeAllConditionVariable', 'rp_condition_all', 4),
    ('SleepConditionVariableSRW', 'rp_condition_srw', 16),
    ('SleepConditionVariableCS', 'rp_condition_cs', 12),
    ('InitOnceInitialize', 'rp_once_init', 4),
    ('InitOnceBeginInitialize', 'rp_once_begin', 16),
    ('InitOnceComplete', 'rp_once_complete', 12),
    ('InitOnceExecuteOnce', 'rp_once_execute', 16),
    ('GetSystemTimeAsFileTime', 'rp_file_time', 4),
    ('IsDBCSLeadByteEx', 'rp_dbcs', 8),
    ('CreateFileW', 'rp_create_file', 28),
    ('CreateMutexW', 'rp_create_mutex', 12),
    ('CreateSemaphoreW', 'rp_create_semaphore', 16),
    ('GetModuleHandleW', 'rp_module_handle', 4),
    ('GetModuleHandleExW', 'rp_module_ex', 12),
    ('LoadLibraryW', 'rp_load', 4),
    ('LoadLibraryExW', 'rp_load_ex', 12),
    ('MultiByteToWideChar', 'rp_multi_wide', 24),
    ('WideCharToMultiByte', 'rp_wide_multi', 32),
    ('GetModuleFileNameW', 'rp_module_name', 12),
    ('GetFileAttributesW', 'rp_attributes', 4),
    ('IsProcessorFeaturePresent', 'rp_processor_feature', 4),
    ('GetFullPathNameW', 'rp_full_path', 16)]
exports = ['LIBRARY RPKERN.DLL', 'EXPORTS']
imports = ['LIBRARY RPKERN.DLL', 'EXPORTS']
for public, private, count in functions:
    exports.append(f' {public}={private}@{count}')
    imports.append(f' {public}@{count}')
Path('BUILD/KERN.DEF').write_text('\n'.join(exports)+'\n')
Path('BUILD/KERNIMP.DEF').write_text('\n'.join(imports)+'\n')
PY
for unit in KERN UTF8 SYNC MODULE FIND; do
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -Wno-cast-function-type -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -c "SRC/$unit.C" -o "BUILD/$unit.O"
done
i686-w64-mingw32-gcc -shared -nostdlib -o /workspace/RPLIB/bin/RPKERN.DLL \
  BUILD/KERN.O BUILD/UTF8.O BUILD/SYNC.O BUILD/MODULE.O BUILD/FIND.O BUILD/KERN.DEF -Wl,--entry,_RPEntry@12,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32
i686-w64-mingw32-dlltool --kill-at --input-def BUILD/KERNIMP.DEF \
  --output-lib /workspace/RPLIB/lib/kern.a
python3 SCRIPTS/AUDIT.PY /workspace/RPLIB/bin
python3 SCRIPTS/UTFDATA.PY
i686-w64-mingw32-gcc -x c -std=c99 -O2 -Wall -Wextra -Werror \
  -march=i386 -mno-sse -mno-sse2 -mno-mmx -fno-builtin \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
  -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -IBUILD -c TESTS/KTEST.C -o BUILD/KTEST.O
i686-w64-mingw32-gcc -nostdlib -o BUILD/KTEST/KTEST.EXE BUILD/KTEST.O \
  /workspace/RPLIB/lib/kern.a -Wl,--entry,_RPEntry@0,--subsystem,windows:3.10 \
  -Wl,--major-os-version,1,--minor-os-version,0,--disable-dynamicbase,--disable-nxcompat \
  -Wl,--no-insert-timestamp -lkernel32 -luser32
cp /workspace/RPLIB/bin/RPKERN.DLL BUILD/KTEST/RPKERN.DLL
python3 SCRIPTS/AUDIT.PY BUILD/KTEST
