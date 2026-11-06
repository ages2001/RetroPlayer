/* CRTDLL filesystem extensions needed by current VLC/MinGW. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <limits.h>
#include <fcntl.h>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>
#include "WIDE.H"

/* CRTDLL predates MSVCRT's 32-bit dev_t. Its mode is at byte 4, not byte 6.
 * Passing the modern MinGW struct silently reads st_nlink as st_mode. */
typedef struct {
    short device;
    unsigned short inode, mode;
    short links, uid, gid, rdevice;
    long size, access_time, modify_time, create_time;
} LEGSTAT;
typedef char check_legacy_stat_size[sizeof(LEGSTAT) == 32 ? 1 : -1];
extern int __cdecl legacy_fstat(int, LEGSTAT *) __asm__("__fstat");

static int file_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
        errno = ENOENT; break;
    case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION:
        errno = EACCES; break;
    case ERROR_FILE_EXISTS: case ERROR_ALREADY_EXISTS: errno = EEXIST; break;
    case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: errno = ENOMEM; break;
    case ERROR_DISK_FULL: errno = ENOSPC; break;
    case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
    case ERROR_DIR_NOT_EMPTY: errno = ENOTEMPTY; break;
    case ERROR_INVALID_HANDLE: errno = EBADF; break;
    case ERROR_NO_MORE_FILES: errno = ENOENT; break;
    case ERROR_NO_UNICODE_TRANSLATION: errno = EILSEQ; break;
    default: errno = EINVAL; break;
    }
    return -1;
}

static BOOL is_9x(void) { return (GetVersion() & 0x80000000UL) != 0; }

int rp_wsopen(const wchar_t *path, int flags, int sharing, ...)
{
    DWORD access, share, creation, attributes=FILE_ATTRIBUTE_NORMAL, error;
    SECURITY_ATTRIBUTES security;
    HANDLE handle;
    int mode=0, descriptor;
    va_list arguments;
    if (!path) { errno=EINVAL; return -1; }
    if (flags & ~(3|_O_APPEND|_O_TEXT|_O_BINARY|_O_CREAT|_O_TRUNC|_O_EXCL|
                  _O_NOINHERIT|_O_TEMPORARY|_O_SHORT_LIVED|_O_SEQUENTIAL|_O_RANDOM)) {
        errno=EINVAL; return -1;
    }
    if (flags & _O_CREAT) {
        va_start(arguments,sharing); mode=va_arg(arguments,int); va_end(arguments);
    }
    if (is_9x()) {
        char *ansi=rp_narrow(path);
        if (!ansi) return file_error(GetLastError());
        /* The Win95 CRT rejects CREAT|EXCL|TRUNC. Exclusive creation already
         * creates an empty file and must never truncate an existing file. */
        if ((flags & (_O_CREAT|_O_EXCL))==(_O_CREAT|_O_EXCL)) flags&=~_O_TRUNC;
        descriptor=_sopen(ansi,flags,sharing,mode);
        error=GetLastError(); rp_free_name(ansi,error);
        return descriptor;
    }
    switch (flags & 3) {
    case _O_RDONLY: access=GENERIC_READ; break;
    case _O_WRONLY: access=GENERIC_WRITE; break;
    case _O_RDWR: access=GENERIC_READ|GENERIC_WRITE; break;
    default: errno=EINVAL; return -1;
    }
    switch (sharing) {
    case _SH_DENYRW: share=0; break;
    case _SH_DENYWR: share=FILE_SHARE_READ; break;
    case _SH_DENYRD: share=FILE_SHARE_WRITE; break;
    case _SH_DENYNO: share=FILE_SHARE_READ|FILE_SHARE_WRITE; break;
    default: errno=EINVAL; return -1;
    }
    if ((flags & _O_TRUNC) && !(access & GENERIC_WRITE)) { errno=EINVAL; return -1; }
    if (flags & _O_CREAT)
        creation=(flags & _O_EXCL) ? CREATE_NEW : (flags & _O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS;
    else creation=(flags & _O_TRUNC) ? TRUNCATE_EXISTING : OPEN_EXISTING;
    if ((flags & _O_CREAT) && !(mode & _S_IWRITE)) attributes=FILE_ATTRIBUTE_READONLY;
    if (flags & _O_TEMPORARY) { attributes|=FILE_FLAG_DELETE_ON_CLOSE; access|=DELETE; share|=FILE_SHARE_DELETE; }
    if (flags & _O_SHORT_LIVED) attributes|=FILE_ATTRIBUTE_TEMPORARY;
    if (flags & _O_SEQUENTIAL) attributes|=FILE_FLAG_SEQUENTIAL_SCAN;
    if (flags & _O_RANDOM) attributes|=FILE_FLAG_RANDOM_ACCESS;
    security.nLength=sizeof(security); security.lpSecurityDescriptor=NULL;
    security.bInheritHandle=(flags & _O_NOINHERIT)==0;
    handle=CreateFileW(path,access,share,&security,creation,attributes,NULL);
    if (handle==INVALID_HANDLE_VALUE) return file_error(GetLastError());
    descriptor=_open_osfhandle((intptr_t)handle,flags);
    if (descriptor==-1) CloseHandle(handle);
    return descriptor;
}

/* Keep descriptors in CRTDLL: FILE objects must belong to the same runtime
 * that supplies fread/fclose. UTF-16 paths never pass through a lossy ACP
 * conversion on NT. */
int rp_wopen(const wchar_t *path, int flags, ...)
{
    int mode=0;
    va_list arguments;
    if (flags & _O_CREAT) {
        va_start(arguments,flags); mode=va_arg(arguments,int); va_end(arguments);
    }
    return rp_wsopen(path,flags,_SH_DENYNO,mode);
}

FILE *rp_wfopen(const wchar_t *path, const wchar_t *mode)
{
    int flags, fd, saved, update=0, binary=0, text=0, exclusive=0;
    char narrow[4];
    unsigned i, count=0;
    FILE *stream;
    if (!path || !mode || !*mode) { errno=EINVAL; return NULL; }
    switch (*mode) {
    case L'r': flags=_O_RDONLY; break;
    case L'w': flags=_O_WRONLY|_O_CREAT|_O_TRUNC; break;
    case L'a': flags=_O_WRONLY|_O_CREAT|_O_APPEND; break;
    default: errno=EINVAL; return NULL;
    }
    narrow[count++]=(char)*mode;
    for (i=1; mode[i]; ++i) {
        switch (mode[i]) {
        case L'+': if (update++) goto invalid; break;
        case L'b': if (binary++ || text) goto invalid; break;
        case L't': if (text++ || binary) goto invalid; break;
        case L'x': if (exclusive++ || *mode!=L'w') goto invalid; break;
        default: goto invalid;
        }
    }
    if (update) { flags=(flags & ~3)|_O_RDWR; narrow[count++]='+'; }
    if (binary) { flags|=_O_BINARY; narrow[count++]='b'; }
    else if (text) { flags|=_O_TEXT; narrow[count++]='t'; }
    if (exclusive) flags|=_O_EXCL;
    narrow[count]=0;
    fd=rp_wsopen(path,flags,_SH_DENYNO,_S_IREAD|_S_IWRITE);
    if (fd<0) return NULL;
    stream=_fdopen(fd,narrow);
    if (!stream) { saved=errno; _close(fd); errno=saved; }
    return stream;
invalid:
    errno=EINVAL; return NULL;
}

static int change_path(const wchar_t *path, unsigned operation)
{
    BOOL result;
    char *ansi;
    DWORD error;
    if (!path) { errno = EINVAL; return -1; }
    if (is_9x()) {
        ansi = rp_narrow(path);
        if (!ansi) return file_error(GetLastError());
        result = operation == 0 ? CreateDirectoryA(ansi, NULL) :
                 operation == 2 ? RemoveDirectoryA(ansi) : DeleteFileA(ansi);
        error = GetLastError(); rp_free_name(ansi, error);
    } else {
        result = operation == 0 ? CreateDirectoryW(path, NULL) :
                 operation == 2 ? RemoveDirectoryW(path) : DeleteFileW(path);
        error = GetLastError();
    }
    return result ? 0 : file_error(error);
}

int rp_wrmdir(const wchar_t *path) { return change_path(path, 2); }

int rp_waccess(const wchar_t *path, int mode)
{
    DWORD attrs;
    if (!path || (mode & ~6)) { errno=EINVAL; return -1; }
    attrs=GetFileAttributesW(path); /* RPKERN supplies the Win9x conversion. */
    if (attrs==INVALID_FILE_ATTRIBUTES) return file_error(GetLastError());
    if ((mode & 2) && !(attrs & FILE_ATTRIBUTE_DIRECTORY) &&
        (attrs & FILE_ATTRIBUTE_READONLY)) { errno=EACCES; return -1; }
    return 0;
}

int rp_wchmod(const wchar_t *path, int mode)
{
    DWORD attrs,error; BOOL ok;
    if (!path || (mode & ~(_S_IREAD|_S_IWRITE))) { errno=EINVAL; return -1; }
    attrs=GetFileAttributesW(path);
    if (attrs==INVALID_FILE_ATTRIBUTES) return file_error(GetLastError());
    if (mode & _S_IWRITE) attrs &= ~FILE_ATTRIBUTE_READONLY;
    else attrs |= FILE_ATTRIBUTE_READONLY;
    if (!attrs) attrs=FILE_ATTRIBUTE_NORMAL;
    if (is_9x()) {
        char *ansi=rp_narrow(path);
        if (!ansi) return file_error(GetLastError());
        ok=SetFileAttributesA(ansi,attrs);error=GetLastError();rp_free_name(ansi,error);
    } else { ok=SetFileAttributesW(path,attrs);error=GetLastError(); }
    return ok?0:file_error(error);
}

int rp_wutime64(const wchar_t *path, struct __utimbuf64 *times)
{
    FILETIME access,modified;ULARGE_INTEGER value;HANDLE file;DWORD error;BOOL ok;
    if (!path) { errno=EINVAL;return -1; }
    if (times) {
        /* Match the 64-bit CRT's supported Unix timestamp range. */
        if (times->actime<0 || times->modtime<0 || times->actime>32535215999LL ||
            times->modtime>32535215999LL) { errno=EINVAL;return -1; }
        value.QuadPart=((unsigned long long)times->actime+11644473600ULL)*10000000ULL;
        access.dwLowDateTime=value.LowPart;access.dwHighDateTime=value.HighPart;
        value.QuadPart=((unsigned long long)times->modtime+11644473600ULL)*10000000ULL;
        modified.dwLowDateTime=value.LowPart;modified.dwHighDateTime=value.HighPart;
    } else { GetSystemTimeAsFileTime(&access);modified=access; }
    if (is_9x()) {
        char *ansi=rp_narrow(path);
        if (!ansi) return file_error(GetLastError());
        file=CreateFileA(ansi,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
                         OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        error=GetLastError();rp_free_name(ansi,error);
    } else {
        file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
                         OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);error=GetLastError();
    }
    if (file==INVALID_HANDLE_VALUE) return file_error(error);
    ok=SetFileTime(file,NULL,&access,&modified);error=GetLastError();CloseHandle(file);
    return ok?0:file_error(error);
}

long long rp_filelengthi64(int descriptor)
{
    DWORD low,high,error;HANDLE file=(HANDLE)_get_osfhandle(descriptor);
    if (file==INVALID_HANDLE_VALUE) { errno=EBADF;return -1; }
    if (GetFileType(file)!=FILE_TYPE_DISK) { errno=EBADF;return -1; }
    SetLastError(NO_ERROR);low=GetFileSize(file,&high);error=GetLastError();
    if (low==INVALID_FILE_SIZE && error) return file_error(error);
    if (high & 0x80000000UL) { errno=EOVERFLOW;return -1; }
    return ((unsigned long long)high<<32)|low;
}

int rp_wmkdir(const wchar_t *path) { return change_path(path, 0); }
int rp_wunlink(const wchar_t *path) { return change_path(path, 1); }
int rp_wremove(const wchar_t *path) { return change_path(path, 1); }

int rp_wrename(const wchar_t *old, const wchar_t *next)
{
    char *from, *to;
    BOOL result;
    DWORD error;
    if (!old || !next) { errno = EINVAL; return -1; }
    if (!is_9x()) {
        result = MoveFileW(old, next); error = GetLastError();
    } else {
        from = rp_narrow(old);
        if (!from) return file_error(GetLastError());
        to = rp_narrow(next);
        if (!to) { error = GetLastError(); rp_free_name(from, error); return file_error(error); }
        result = MoveFileA(from, to); error = GetLastError();
        rp_free_name(from, error); rp_free_name(to, error);
    }
    return result ? 0 : file_error(error);
}

/* Query into a private buffer, then honor the CRT caller's allocation rules. */
static wchar_t *full_path(wchar_t *output, const wchar_t *path, size_t capacity)
{
    WCHAR *wide = NULL, *result = output;
    char *ansi = NULL, *absolute = NULL;
    DWORD required, got, error;
    int units;
    size_t i;
    BOOL current = !path || !*path;
    if (output && !capacity) { errno = EINVAL; return NULL; }
    if (is_9x()) {
        if (!current && !(ansi = rp_narrow(path))) { file_error(GetLastError()); return NULL; }
        required = current ? GetCurrentDirectoryA(0, NULL) : GetFullPathNameA(ansi, 0, NULL, NULL);
        if (!required) goto windows_failure;
        absolute = malloc(required);
        if (!absolute) goto memory_failure;
        got = current ? GetCurrentDirectoryA(required, absolute) : GetFullPathNameA(ansi, required, absolute, NULL);
        if (!got) goto windows_failure;
        if (got >= required) { errno = ERANGE; goto failure; }
        units = MultiByteToWideChar(CP_ACP, 0, absolute, -1, NULL, 0);
        if (!units) goto windows_failure;
        required = (DWORD)units;
        wide = malloc((size_t)required * sizeof(WCHAR));
        if (!wide) goto memory_failure;
        if (!MultiByteToWideChar(CP_ACP, 0, absolute, -1, wide, units)) goto windows_failure;
    } else {
        required = current ? GetCurrentDirectoryW(0, NULL) : GetFullPathNameW(path, 0, NULL, NULL);
        if (!required) goto windows_failure;
        if (required > INT_MAX / sizeof(WCHAR)) { errno = ERANGE; goto failure; }
        wide = malloc((size_t)required * sizeof(WCHAR));
        if (!wide) goto memory_failure;
        got = current ? GetCurrentDirectoryW(required, wide) : GetFullPathNameW(path, required, wide, NULL);
        if (!got) goto windows_failure;
        if (got >= required) { errno = ERANGE; goto failure; }
        required = got + 1;
    }
    if (capacity && capacity < required) { errno = ERANGE; goto failure; }
    if (!result) {
        size_t allocated = capacity ? capacity : required;
        if (allocated > INT_MAX / sizeof(WCHAR)) { errno = ERANGE; goto failure; }
        result = malloc(allocated * sizeof(WCHAR));
        if (!result) goto memory_failure;
    }
    for (i = 0; i < required; ++i) result[i] = wide[i];
    free(wide); free(absolute); rp_free_name(ansi, GetLastError());
    return result;
memory_failure:
    errno = ENOMEM;
    goto failure;
windows_failure:
    error = GetLastError(); file_error(error);
failure:
    free(wide); free(absolute); rp_free_name(ansi, GetLastError());
    return NULL;
}

wchar_t *rp_wgetcwd(wchar_t *output, int capacity)
{
    if (capacity < 0) { errno = EINVAL; return NULL; }
    return full_path(output, NULL, (size_t)capacity);
}
wchar_t *rp_wfullpath(wchar_t *output, const wchar_t *path, size_t capacity)
{
    return full_path(output, path, capacity);
}

static long long unix_time64(FILETIME value)
{
    ULARGE_INTEGER ticks;
    unsigned long long seconds;
    ticks.LowPart = value.dwLowDateTime; ticks.HighPart = value.dwHighDateTime;
    if (ticks.QuadPart < 116444736000000000ULL) return -1;
    seconds = (ticks.QuadPart - 116444736000000000ULL) / 10000000ULL;
    return (long long)seconds;
}

static long unix_time(FILETIME value)
{
    long long seconds=unix_time64(value);
    return seconds >= 0 && seconds <= LONG_MAX ? (long)seconds : -1;
}

long long rp_lseeki64(int descriptor, long long offset, int origin)
{
    HANDLE handle;
    LARGE_INTEGER position;
    DWORD low, error;
    if (origin < 0 || origin > 2) { errno = EINVAL; return -1; }
    handle = (HANDLE)_get_osfhandle(descriptor);
    if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    if (GetFileType(handle) != FILE_TYPE_DISK) { errno = ESPIPE; return -1; }
    position.QuadPart = offset;
    SetLastError(NO_ERROR);
    low = SetFilePointer(handle, position.LowPart, &position.HighPart, (DWORD)origin);
    error = GetLastError();
    if (low == INVALID_SET_FILE_POINTER && error != NO_ERROR) {
        file_error(error); return -1;
    }
    position.LowPart = low;
    return position.QuadPart;
}

static BOOL widen_find(const WIN32_FIND_DATAA *input, WIN32_FIND_DATAW *output)
{
    output->dwFileAttributes = input->dwFileAttributes;
    output->ftCreationTime = input->ftCreationTime;
    output->ftLastAccessTime = input->ftLastAccessTime;
    output->ftLastWriteTime = input->ftLastWriteTime;
    output->nFileSizeHigh = input->nFileSizeHigh;
    output->nFileSizeLow = input->nFileSizeLow;
    return MultiByteToWideChar(CP_ACP, 0, input->cFileName, -1, output->cFileName, MAX_PATH) != 0;
}

static HANDLE find_first(const wchar_t *path, WIN32_FIND_DATAW *data)
{
    char *ansi;
    HANDLE handle;
    WIN32_FIND_DATAA narrow;
    DWORD error;
    if (!is_9x()) return FindFirstFileW(path, data);
    ansi = rp_narrow(path);
    if (!ansi) return INVALID_HANDLE_VALUE;
    handle = FindFirstFileA(ansi, &narrow);
    error = GetLastError(); rp_free_name(ansi, error);
    if (handle != INVALID_HANDLE_VALUE && !widen_find(&narrow, data)) {
        error = GetLastError(); FindClose(handle); SetLastError(error);
        return INVALID_HANDLE_VALUE;
    }
    return handle;
}

static void fill_find(const WIN32_FIND_DATAW *from, struct _wfinddata32_t *to)
{
    unsigned i;
    to->attrib = from->dwFileAttributes & 0x37;
    to->time_create = unix_time(from->ftCreationTime);
    to->time_access = unix_time(from->ftLastAccessTime);
    to->time_write = unix_time(from->ftLastWriteTime);
    to->size = from->nFileSizeLow;
    for (i = 0; i < MAX_PATH-1 && from->cFileName[i]; ++i) to->name[i] = from->cFileName[i];
    to->name[i] = 0;
}

static void fill_find_a(const WIN32_FIND_DATAA *from, struct _finddata32_t *to)
{
    unsigned i;
    to->attrib = from->dwFileAttributes & 0x37;
    to->time_create = unix_time(from->ftCreationTime);
    to->time_access = unix_time(from->ftLastAccessTime);
    to->time_write = unix_time(from->ftLastWriteTime);
    to->size = from->nFileSizeLow;
    for (i = 0; i < MAX_PATH-1 && from->cFileName[i]; ++i)
        to->name[i] = from->cFileName[i];
    to->name[i] = 0;
}

intptr_t rp_findfirst(const char *pattern, struct _finddata32_t *data)
{
    WIN32_FIND_DATAA info;
    HANDLE handle;
    if (!pattern || !data) { errno = EINVAL; return -1; }
    handle = FindFirstFileA(pattern, &info);
    if (handle == INVALID_HANDLE_VALUE) { file_error(GetLastError()); return -1; }
    fill_find_a(&info, data);
    return (intptr_t)handle;
}

int rp_findnext(intptr_t value, struct _finddata32_t *data)
{
    WIN32_FIND_DATAA info;
    if (!data) { errno = EINVAL; return -1; }
    if (!FindNextFileA((HANDLE)value, &info)) return file_error(GetLastError());
    fill_find_a(&info, data);
    return 0;
}

intptr_t rp_wfindfirst(const wchar_t *pattern, struct _wfinddata32_t *data)
{
    WIN32_FIND_DATAW info;
    HANDLE handle;
    if (!pattern || !data) { errno = EINVAL; return -1; }
    handle = find_first(pattern, &info);
    if (handle == INVALID_HANDLE_VALUE) { file_error(GetLastError()); return -1; }
    fill_find(&info, data);
    return (intptr_t)handle;
}

int rp_wfindnext(intptr_t value, struct _wfinddata32_t *data)
{
    WIN32_FIND_DATAW info;
    WIN32_FIND_DATAA narrow;
    BOOL result;
    if (!data) { errno = EINVAL; return -1; }
    if (!is_9x()) result = FindNextFileW((HANDLE)value, &info);
    else {
        result = FindNextFileA((HANDLE)value, &narrow);
        if (result) result = widen_find(&narrow, &info);
    }
    if (!result) return file_error(GetLastError());
    fill_find(&info, data);
    return 0;
}

int rp_findclose(intptr_t value)
{
    return FindClose((HANDLE)value) ? 0 : file_error(GetLastError());
}

int rp_fstati64(int fd, struct _stat32i64 *result)
{
    LEGSTAT old;
    HANDLE handle;
    DWORD low, high, error;
    if (!result) { errno = EINVAL; return -1; }
    if (legacy_fstat(fd, &old)) return -1;
    result->st_dev = old.device; result->st_ino = old.inode;
    result->st_mode = old.mode; result->st_nlink = old.links;
    result->st_uid = old.uid; result->st_gid = old.gid;
    result->st_rdev = old.rdevice; result->st_size = old.size;
    result->st_atime = old.access_time; result->st_mtime = old.modify_time;
    result->st_ctime = old.create_time;
    handle = (HANDLE)_get_osfhandle(fd);
    if (GetFileType(handle) != FILE_TYPE_DISK) return 0;
    SetLastError(0);
    low = GetFileSize(handle, &high); error = GetLastError();
    if (low == INVALID_FILE_SIZE && error) return file_error(error);
    result->st_size = ((unsigned long long)high << 32) | low;
    return 0;
}

int rp_wstat64(const wchar_t *path, struct _stat64 *result)
{
    WIN32_FIND_DATAW info;
    HANDLE handle;
    WCHAR *absolute;
    DWORD attrs, error;
    char *ansi;
    size_t length = 0, i;
    unsigned short permission;
    if (!path || !result) { errno = EINVAL; return -1; }
    if (!*path) { errno = ENOENT; return -1; }
    while (path[length]) {
        if (path[length] == L'*' || path[length] == L'?') { errno = ENOENT; return -1; }
        ++length;
    }
    absolute = full_path(NULL, path, 0);
    if (!absolute) return -1;
    length = 0;
    while (absolute[length]) ++length;
    while (length > 3 && (absolute[length-1] == L'\\' || absolute[length-1] == L'/'))
        absolute[--length] = 0;
    handle = find_first(absolute, &info);
    if (handle == INVALID_HANDLE_VALUE) {
        /* Root directories have no FindFirstFile entry on FAT. As with the
         * legacy CRT, their size and unavailable timestamps are zero. */
        if (is_9x()) {
            ansi = rp_narrow(absolute);
            if (!ansi) { free(absolute); return file_error(GetLastError()); }
            attrs = GetFileAttributesA(ansi); error = GetLastError();
            rp_free_name(ansi, error);
        } else { attrs = GetFileAttributesW(absolute); error = GetLastError(); }
        if (attrs == INVALID_FILE_ATTRIBUTES) { free(absolute); return file_error(error); }
        if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) { free(absolute); errno = ENOENT; return -1; }
        info.dwFileAttributes = attrs;
        info.nFileSizeLow = info.nFileSizeHigh = 0;
        info.ftCreationTime.dwLowDateTime = info.ftCreationTime.dwHighDateTime = 0;
        info.ftLastAccessTime = info.ftLastWriteTime = info.ftCreationTime;
    } else FindClose(handle);
    result->st_dev = 0;
    if (length >= 2 && absolute[1] == L':') {
        WCHAR drive = absolute[0];
        if (drive >= L'a' && drive <= L'z') drive -= L'a'-L'A';
        if (drive >= L'A' && drive <= L'Z') result->st_dev = drive-L'A';
    }
    result->st_rdev = result->st_dev;
    result->st_ino = result->st_uid = result->st_gid = 0;
    result->st_nlink = 1;
    permission = _S_IREAD | ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : _S_IWRITE);
    if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        result->st_mode = _S_IFDIR;
        permission |= _S_IEXEC;
        result->st_size = 0;
    } else {
        static const WCHAR *extensions[] = {L".exe", L".com", L".bat", L".cmd"};
        result->st_mode = _S_IFREG;
        if (info.nFileSizeHigh & 0x80000000UL) { free(absolute); errno = EOVERFLOW; return -1; }
        result->st_size = ((unsigned long long)info.nFileSizeHigh << 32) | info.nFileSizeLow;
        if (length >= 4) for (i = 0; i < 4; ++i) {
            unsigned j;
            for (j = 0; j < 4; ++j) {
                WCHAR c = absolute[length-4+j];
                if (c >= L'A' && c <= L'Z') c += L'a'-L'A';
                if (c != extensions[i][j]) break;
            }
            if (j == 4) { permission |= _S_IEXEC; break; }
        }
    }
    result->st_mode |= permission | (permission >> 3) | (permission >> 6);
    result->st_mtime = unix_time64(info.ftLastWriteTime);
    result->st_atime = unix_time64(info.ftLastAccessTime);
    result->st_ctime = unix_time64(info.ftCreationTime);
    if (result->st_mtime == -1) result->st_mtime = 0;
    if (result->st_atime == -1) result->st_atime = result->st_mtime;
    if (result->st_ctime == -1) result->st_ctime = result->st_mtime;
    free(absolute);
    return 0;
}

int rp_stat64(const char *path, struct _stat64 *result)
{
    int units, status;
    WCHAR *wide;
    if (!path || !result) { errno = EINVAL; return -1; }
    units = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
    if (!units) return file_error(GetLastError());
    wide = malloc((size_t)units * sizeof(WCHAR));
    if (!wide) { errno = ENOMEM; return -1; }
    if (!MultiByteToWideChar(CP_ACP, 0, path, -1, wide, units)) {
        DWORD error = GetLastError(); free(wide); return file_error(error);
    }
    status = rp_wstat64(wide, result);
    free(wide);
    return status;
}

/* The public CRT layouts differ in alignment as well as timestamp width. */
static void narrow_stat(const struct _stat64 *from, struct _stat32i64 *to)
{
    to->st_dev=from->st_dev; to->st_ino=from->st_ino; to->st_mode=from->st_mode;
    to->st_nlink=from->st_nlink; to->st_uid=from->st_uid; to->st_gid=from->st_gid;
    to->st_rdev=from->st_rdev; to->st_size=from->st_size;
    to->st_atime=from->st_atime>=0 && from->st_atime<=LONG_MAX ? (long)from->st_atime : -1;
    to->st_mtime=from->st_mtime>=0 && from->st_mtime<=LONG_MAX ? (long)from->st_mtime : -1;
    to->st_ctime=from->st_ctime>=0 && from->st_ctime<=LONG_MAX ? (long)from->st_ctime : -1;
}

int rp_wstati64(const wchar_t *path, struct _stat32i64 *result)
{
    struct _stat64 value;
    if (!result) { errno=EINVAL; return -1; }
    if (rp_wstat64(path,&value)) return -1;
    narrow_stat(&value,result); return 0;
}

int rp_wstat32(const wchar_t *path, struct _stat32 *result)
{
    struct _stat64 value;
    struct _stat32 narrowed;
    if (!result) { errno=EINVAL; return -1; }
    if (rp_wstat64(path,&value)) return -1;
    if (value.st_size>LONG_MAX || value.st_atime>LONG_MAX ||
        value.st_mtime>LONG_MAX || value.st_ctime>LONG_MAX) {
        errno=EOVERFLOW; return -1;
    }
    narrowed.st_dev=value.st_dev; narrowed.st_ino=value.st_ino;
    narrowed.st_mode=value.st_mode; narrowed.st_nlink=value.st_nlink;
    narrowed.st_uid=value.st_uid; narrowed.st_gid=value.st_gid;
    narrowed.st_rdev=value.st_rdev; narrowed.st_size=(long)value.st_size;
    narrowed.st_atime=(long)value.st_atime; narrowed.st_mtime=(long)value.st_mtime;
    narrowed.st_ctime=(long)value.st_ctime;
    *result=narrowed; return 0;
}

int rp_stati64(const char *path, struct _stat32i64 *result)
{
    struct _stat64 value;
    if (!result) { errno=EINVAL; return -1; }
    if (rp_stat64(path,&value)) return -1;
    narrow_stat(&value,result); return 0;
}

int rp_fstat64(int descriptor, struct _stat64 *result)
{
    struct _stat32i64 old;
    FILETIME created, accessed, modified;
    HANDLE handle;
    if (!result) { errno=EINVAL; return -1; }
    if (rp_fstati64(descriptor,&old)) return -1;
    result->st_dev=old.st_dev; result->st_ino=old.st_ino; result->st_mode=old.st_mode;
    result->st_nlink=old.st_nlink; result->st_uid=old.st_uid; result->st_gid=old.st_gid;
    result->st_rdev=old.st_rdev; result->st_size=old.st_size;
    result->st_atime=old.st_atime; result->st_mtime=old.st_mtime; result->st_ctime=old.st_ctime;
    handle=(HANDLE)_get_osfhandle(descriptor);
    if (GetFileType(handle)!=FILE_TYPE_DISK) return 0;
    if (!GetFileTime(handle,&created,&accessed,&modified)) return file_error(GetLastError());
    result->st_mtime=unix_time64(modified);
    result->st_atime=unix_time64(accessed);
    result->st_ctime=unix_time64(created);
    if (result->st_mtime==-1) result->st_mtime=0;
    if (result->st_atime==-1) result->st_atime=result->st_mtime;
    if (result->st_ctime==-1) result->st_ctime=result->st_mtime;
    return 0;
}
