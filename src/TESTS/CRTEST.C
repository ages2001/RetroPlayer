#define WIN32_LEAN_AND_MEAN
#define _CRT_RAND_S
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <share.h>

static unsigned checks, failures, first_failure;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; if (!first_failure) first_failure=checks; } } while (0)
static char module[MAX_PATH], original[MAX_PATH], folder[MAX_PATH];
static WCHAR wide[MAX_PATH];
extern int __cdecl RPSetJmp(void *) __attribute__((returns_twice));
extern void __cdecl RPLongJmp(void *,int) __attribute__((noreturn));
static void __attribute__((noinline)) jump_helper(void *target,int value)
{
    volatile unsigned stack[64];
    for(unsigned i=0;i<64;++i) stack[i]=i;
    RPLongJmp(target,value+(int)stack[0]);
}

static int __attribute__((noinline)) jump_roundtrip(int wanted)
{
    unsigned context[16];volatile unsigned visits=0;
    int returned=RPSetJmp(context);
    if(!returned) { ++visits;jump_helper(context,wanted); }
    return returned==(wanted?wanted:1) && visits==1;
}

static int wide_equal(const WCHAR *a, const WCHAR *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static unsigned wide_length(const WCHAR *text)
{
    unsigned size = 0;
    while (text[size]) ++size;
    return size;
}

void WINAPI RPEntry(void)
{
    char *p, *last, text[256], bytes[64], reference[64];
    DWORD length, size, got;
    HANDLE output;
    int fd, other, status;
    int fstat_status = -99, fstat_error = 0;
    int wide_open_error=0, native_fd=-99, native_open_error=0;
    unsigned fstat_mode = 0, fstat_low = 0, fstat_high = 0;
    LONG native_high;
    DWORD native_low, native_error;
    long long large_seek;
    intptr_t search;
    struct _wfinddata32_t found;
    struct _finddata32_t narrow_found;
    struct _stat32i64 info;
    struct _stat64 info64;
    struct _stat32 short_stat;
    ULARGE_INTEGER timestamp;
    FILETIME future;
    struct tm date;
    long instant, now;
    wchar_t *allocated;
    FILE *stream;
    static const wchar_t message[] = {L'A', 0xe9, L'\n', 0};
    unsigned i;
    unsigned alignment, j;
    unsigned char *aligned_block, *resized;
    length = GetModuleFileNameA(NULL, module, MAX_PATH);
    if (!length || length >= MAX_PATH-16) ExitProcess(2);
    last = module;
    for (p = module; *p; ++p) if (*p == '\\') last = p+1;
    *last = 0;
    if (!GetCurrentDirectoryA(MAX_PATH, original)) ExitProcess(3);
    if (!GetTempFileNameA(module, "RCT", 0, folder)) ExitProcess(4);
    if (!DeleteFileA(folder) || !CreateDirectoryA(folder, NULL)) ExitProcess(5);
    if (!SetCurrentDirectoryA(folder)) ExitProcess(6);
    {
        unsigned entropy=0xdeadbeef;
        CHECK(rand_s(NULL)==EINVAL && errno==EINVAL);
        int entropy_status=rand_s(&entropy);
        CHECK(entropy_status==0 || (entropy_status==ENOSYS && errno==ENOSYS && entropy==0));
    }
    CHECK(jump_roundtrip(42));
    CHECK(jump_roundtrip(0));

    {
        struct _stat32 small;
        const wchar_t *name=L"F\x00e9.TXT";
        CHECK(_wfopen(NULL,L"rb")==NULL && errno==EINVAL);
        CHECK(_wfopen(name,L"rbb")==NULL && errno==EINVAL);
        CHECK(_wfopen(name,L"rbt")==NULL && errno==EINVAL);
        CHECK(_wfopen(name,L"rx")==NULL && errno==EINVAL);
        CHECK(_wopen(NULL,_O_RDONLY)==-1 && errno==EINVAL);
        CHECK(_wstat32(NULL,&small)==-1 && errno==EINVAL);
        stream=_wfopen(name,L"w+bx");
        wide_open_error=errno;
        native_fd=_wsopen(L"NATIVE.TXT",_O_CREAT|_O_EXCL|_O_TRUNC|_O_RDWR|_O_BINARY,_SH_DENYNO,_S_IREAD|_S_IWRITE);
        native_open_error=errno;
        if(native_fd>=0) _close(native_fd);
        _wremove(L"NATIVE.TXT");
        CHECK(stream!=NULL);
        if (stream) {
            CHECK(fwrite("A\r\nB",1,4,stream)==4);
            CHECK(fseek(stream,0,SEEK_SET)==0);
            CHECK(fread(bytes,1,4,stream)==4 && bytes[1]=='\r' && bytes[3]=='B');
            CHECK(fclose(stream)==0);
        }
        CHECK(_wfopen(name,L"wbx")==NULL && errno==EEXIST);
        CHECK(_wstat32(name,&small)==0 && small.st_size==4 && (small.st_mode&_S_IFMT)==_S_IFREG);
        stream=_wfopen(name,L"ab");
        CHECK(stream!=NULL);
        if (stream) { CHECK(fwrite("C",1,1,stream)==1); CHECK(fclose(stream)==0); }
        fd=_wopen(name,_O_RDONLY|_O_BINARY);
        CHECK(fd>=0);
        if(fd>=0) { CHECK(_read(fd,bytes,8)==5 && bytes[4]=='C'); CHECK(_close(fd)==0); }
        CHECK(_wremove(name)==0);
        stream=_wfopen(name,L"wt");
        CHECK(stream!=NULL);
        if(stream) { CHECK(fwrite("A\nB",1,3,stream)==3); CHECK(fclose(stream)==0); }
        stream=_wfopen(name,L"rt");
        CHECK(stream!=NULL);
        if(stream) { CHECK(fread(bytes,1,8,stream)==3 && bytes[1]=='\n'); CHECK(fclose(stream)==0); }
        CHECK(_wremove(name)==0);
    }

    CHECK(_wgetenv(NULL)==NULL && errno==EINVAL);
    CHECK(_wgetenv(L"RPABSENT")==NULL);
    CHECK(_putenv("RPENV=first")==0);
    allocated=_wgetenv(L"RPENV");
    CHECK(allocated && wide_equal(allocated,L"first"));
    CHECK(_wgetenv(L"RPENV")==allocated);
    CHECK(_putenv("RPENV=a longer replacement")==0);
    allocated=_wgetenv(L"rpenv");
    CHECK(allocated && wide_equal(allocated,L"a longer replacement"));
    CHECK(_putenv("RPENV=x")==0);
    allocated=_wgetenv(L"RPENV");
    CHECK(allocated && wide_equal(allocated,L"x"));
    CHECK(_putenv("RPENV=")==0);
    CHECK(_wgetenv(L"RPENV")==NULL);

    CHECK(_waccess(NULL,0)==-1 && errno==EINVAL);
    CHECK(_waccess(L"MISSING",0)==-1 && errno==ENOENT);
    CHECK(_waccess(L".",1)==-1 && errno==EINVAL);
    CHECK(_wmkdir(L"EMPTY")==0);
    CHECK(_waccess(L"EMPTY",6)==0);
    CHECK(_wrmdir(L"EMPTY")==0);
    CHECK(_wrmdir(L"EMPTY")==-1 && errno==ENOENT);
    CHECK(_wrmdir(NULL)==-1 && errno==EINVAL);

    fd=_open("FUTURE.BIN",_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY,_S_IREAD|_S_IWRITE);
    CHECK(fd>=0);
    if (fd>=0) {
        CHECK(_write(fd,"2040",4)==4);
        CHECK(_filelengthi64(fd)==4);
        CHECK(_lseeki64(fd,0,SEEK_CUR)==4); /* Length must not move the cursor. */
        timestamp.QuadPart=138534624000000000ULL; /* 2040-01-01 UTC */
        future.dwLowDateTime=timestamp.LowPart; future.dwHighDateTime=timestamp.HighPart;
        CHECK(SetFileTime((HANDLE)_get_osfhandle(fd),NULL,NULL,&future));
        CHECK(_fstat64(fd,&info64)==0 && info64.st_size==4 && info64.st_mtime==2208988800LL);
        CHECK(_close(fd)==0);
        CHECK(_stat64("FUTURE.BIN",&info64)==0 && info64.st_size==4 && info64.st_mtime==2208988800LL);
        CHECK(_wstat64(L"FUTURE.BIN",&info64)==0 && info64.st_mtime==2208988800LL);
        CHECK(_wstat32(L"FUTURE.BIN",&short_stat)==-1 && errno==EOVERFLOW);
        CHECK(_waccess(L"FUTURE.BIN",6)==0);
        CHECK(SetFileAttributesA("FUTURE.BIN",FILE_ATTRIBUTE_READONLY));
        CHECK(_waccess(L"FUTURE.BIN",4)==0);
        CHECK(_waccess(L"FUTURE.BIN",2)==-1 && errno==EACCES);
        CHECK(SetFileAttributesA("FUTURE.BIN",FILE_ATTRIBUTE_NORMAL));
        CHECK(_wchmod(L"FUTURE.BIN",_S_IREAD)==0);
        CHECK(_waccess(L"FUTURE.BIN",2)==-1 && errno==EACCES);
        CHECK(_wchmod(L"FUTURE.BIN",_S_IREAD|_S_IWRITE)==0);
        struct __utimbuf64 times={2208988800LL,2208988802LL};
        CHECK(_wutime64(L"FUTURE.BIN",&times)==0);
        CHECK(_wstat64(L"FUTURE.BIN",&info64)==0 && info64.st_mtime==2208988802LL && info64.st_size==4);
        times.modtime=-1;
        CHECK(_wutime64(L"FUTURE.BIN",&times)==-1 && errno==EINVAL);
        CHECK(_wstat64(L"FUTURE.BIN",&info64)==0 && info64.st_mtime==2208988802LL);
        CHECK(_wutime64(L"FUTURE.BIN",NULL)==0);
        CHECK(_wstat64(L"FUTURE.BIN",&info64)==0 && info64.st_mtime>1700000000LL && info64.st_mtime<2208988800LL);
        CHECK(_wunlink(L"FUTURE.BIN")==0);
    }
    CHECK(_stat64("MISSING",&info64)==-1 && errno==ENOENT);
    CHECK(_wstat64(NULL,&info64)==-1 && errno==EINVAL);
    CHECK(_fstat64(-1,&info64)==-1 && errno==EBADF);
    CHECK(_filelengthi64(-1)==-1 && errno==EBADF);
    CHECK(_wchmod(NULL,_S_IREAD)==-1 && errno==EINVAL);
    CHECK(_wchmod(L"MISSING",_S_IREAD)==-1 && errno==ENOENT);
    CHECK(_wutime64(L"MISSING",NULL)==-1 && errno==ENOENT);

    for (alignment=1; alignment<=4096; alignment*=2) {
        aligned_block=_aligned_malloc(257,alignment);
        CHECK(aligned_block && !((uintptr_t)aligned_block & (alignment-1)));
        if (!aligned_block) continue;
        for (j=0; j<257; ++j) aligned_block[j]=(unsigned char)(j*17);
        resized=_aligned_realloc(aligned_block,8193,alignment);
        CHECK(resized && !((uintptr_t)resized & (alignment-1)));
        if (!resized) { _aligned_free(aligned_block); continue; }
        for (j=0; j<257; ++j) CHECK(resized[j]==(unsigned char)(j*17));
        aligned_block=_aligned_realloc(resized,17,alignment);
        CHECK(aligned_block && !((uintptr_t)aligned_block & (alignment-1)));
        if (!aligned_block) { _aligned_free(resized); continue; }
        for (j=0; j<17; ++j) CHECK(aligned_block[j]==(unsigned char)(j*17));
        CHECK(_aligned_realloc(aligned_block,SIZE_MAX,alignment)==NULL && errno==ENOMEM);
        CHECK(aligned_block[16]==(unsigned char)(16*17));
        CHECK(_aligned_realloc(aligned_block,33,alignment*2)==NULL && errno==EINVAL);
        CHECK(_aligned_realloc(aligned_block,0,alignment)==NULL);
    }
    CHECK(_aligned_malloc(1,0)==NULL && errno==EINVAL);
    CHECK(_aligned_malloc(1,3)==NULL && errno==EINVAL);
    CHECK(_aligned_malloc(0,16)==NULL && errno==EINVAL);
    _aligned_free(NULL);

    fd=_wsopen(L"W\x00e9.BIN",_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY|_O_NOINHERIT,
               _SH_DENYWR,_S_IREAD|_S_IWRITE);
    CHECK(fd>=0);
    if (fd>=0) {
        CHECK(_write(fd,"ABCDE",5)==5);
        CHECK(_wsopen(L"W\x00e9.BIN",_O_WRONLY|_O_BINARY,_SH_DENYNO)==-1 && errno==EACCES);
        CHECK(_lseeki64(fd,0,SEEK_SET)==0);
        CHECK(_read(fd,bytes,5)==5 && bytes[0]=='A' && bytes[4]=='E');
        CHECK(_close(fd)==0);
    }
    CHECK(_wsopen(L"W\x00e9.BIN",_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,
                  _SH_DENYNO,_S_IREAD|_S_IWRITE)==-1 && errno==EEXIST);
    fd=_wsopen(L"W\x00e9.BIN",_O_WRONLY|_O_BINARY|_O_TRUNC,_SH_DENYNO);
    CHECK(fd>=0);
    if (fd>=0) { CHECK(_fstati64(fd,&info)==0 && info.st_size==0); CHECK(_close(fd)==0); }
    CHECK(_wunlink(L"W\x00e9.BIN")==0);
    CHECK(_wsopen(NULL,_O_RDONLY,_SH_DENYNO)==-1 && errno==EINVAL);

    instant = 0;
    CHECK(_gmtime32_s(&date, &instant) == 0);
    CHECK(date.tm_year == 70 && date.tm_mon == 0 && date.tm_mday == 1 && date.tm_wday == 4);
    instant = 951782400L; /* 2000-02-29 00:00:00 UTC */
    CHECK(_gmtime32_s(&date, &instant) == 0);
    CHECK(date.tm_year == 100 && date.tm_mon == 1 && date.tm_mday == 29 && date.tm_yday == 59);
    instant = 2147483647L;
    CHECK(_gmtime32_s(&date, &instant) == 0);
    CHECK(date.tm_year == 138 && date.tm_mon == 0 && date.tm_mday == 19 && date.tm_sec == 7);
    instant = -1;
    CHECK(_gmtime32_s(&date, &instant) == EINVAL && date.tm_year == -1 && errno == EINVAL);
    CHECK(_gmtime32_s(NULL, &instant) == EINVAL);
    CHECK(_localtime32_s(&date, NULL) == EINVAL && date.tm_sec == -1);
    now = _time32(&instant);
    CHECK(now == instant && instant > 1700000000L);
    CHECK(_localtime32_s(&date, &instant) == 0 && date.tm_year >= 123);
    CHECK(_mktime32(&date) == instant);
    instant = 951825600L; /* Leap day, well away from local DST transitions. */
    CHECK(_localtime32_s(&date, &instant) == 0);
    date.tm_mday += 1;
    CHECK(_mktime32(&date) == instant + 86400L);
    CHECK(date.tm_mon == 2 && date.tm_mday == 1);
    CHECK(_mktime32(NULL) == -1 && errno == EINVAL);

    allocated = _wgetcwd(NULL, 0);
    CHECK(allocated != NULL);
    CHECK(MultiByteToWideChar(CP_ACP, 0, folder, -1, wide, MAX_PATH) != 0);
    CHECK(allocated && wide_equal(allocated, wide));
    free(allocated);
    wide[0] = 0x1234;
    CHECK(_wgetcwd(wide, 1) == NULL && errno == ERANGE && wide[0] == 0x1234);
    CHECK(_wgetcwd(wide, -1) == NULL && errno == EINVAL);
    allocated = _wfullpath(NULL, L".\\SMALL.BIN", 0);
    CHECK(allocated != NULL);
    if (allocated) {
        length = wide_length(allocated);
        CHECK(length >= 9 && wide_equal(allocated+length-9, L"SMALL.BIN"));
    }
    free(allocated);
    CHECK(_wmkdir(L"SUBDIR") == 0);
    CHECK(_wmkdir(L"SUBDIR") == -1 && errno == EEXIST);
    status = _wstati64(L"SUBDIR", &info);
    CHECK(status == 0 && (info.st_mode & _S_IFMT) == _S_IFDIR);
    status = _wstati64(L"C:\\", &info);
    CHECK(status == 0 && (info.st_mode & _S_IFMT) == _S_IFDIR);

    fd = _open("SMALL.BIN", _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
    CHECK(fd >= 0);
    if (fd >= 0) {
        CHECK(_write(fd, "RPCRT123", 8) == 8);
        CHECK(_lseeki64(fd, 3, SEEK_SET) == 3);
        CHECK(_read(fd, bytes, 2) == 2 && bytes[0] == 'R' && bytes[1] == 'T');
        CHECK(_lseeki64(fd, -2, SEEK_END) == 6);
        CHECK(_lseeki64(fd, 0, SEEK_CUR) == 6);
        CHECK(_lseeki64(fd, -1, SEEK_SET) == -1);
        /* Compare with the native filesystem's limit. Win95 may reject a
         * high offset even though the arithmetic in the adapter is 64-bit. */
        native_high=1; SetLastError(NO_ERROR);
        native_low=SetFilePointer((HANDLE)_get_osfhandle(fd),5,&native_high,FILE_BEGIN);
        native_error=GetLastError();
        CHECK(_lseeki64(fd,0,SEEK_SET)==0);
        large_seek=_lseeki64(fd,0x100000005LL,SEEK_SET);
        if (native_low==INVALID_SET_FILE_POINTER && native_error!=NO_ERROR)
            CHECK(large_seek==-1);
        else {
            CHECK(large_seek==((long long)(unsigned long)native_high<<32 | native_low));
            CHECK(_lseeki64(fd,0,SEEK_CUR)==large_seek);
        }
        CHECK(_lseeki64(fd, 0, SEEK_SET) == 0);
        CHECK(_lseeki64(fd, 0, 99) == -1 && errno == EINVAL);
        fstat_status = _fstati64(fd, &info);
        fstat_error = errno;
        fstat_mode = info.st_mode;
        fstat_low = (unsigned)info.st_size;
        fstat_high = (unsigned)((unsigned long long)info.st_size >> 32);
        CHECK(fstat_status == 0 && info.st_size == 8 && (info.st_mode & _S_IFMT) == _S_IFREG);
        CHECK(_close(fd) == 0);
    }
    CHECK(_stati64("SMALL.BIN", &info) == 0 && info.st_size == 8);
    CHECK(_wstati64(L"SMALL.BIN", &info) == 0 && info.st_size == 8);
    CHECK(_wstati64(L"*.BIN", &info) == -1 && errno == ENOENT);
    CHECK(_wstati64(L"MISSING", &info) == -1 && errno == ENOENT);
    search = _wfindfirst32(L"*.BIN", &found);
    CHECK(search != -1);
    if (search != -1) {
        CHECK(wide_equal(found.name, L"SMALL.BIN") && found.size == 8);
        CHECK(_wfindnext32(search, &found) == -1 && errno == ENOENT);
        CHECK(_findclose(search) == 0);
    }
    CHECK(_wrename(L"SMALL.BIN", L"MOVED.BIN") == 0);
    search = _findfirst32("*.BIN", &narrow_found);
    CHECK(search != -1);
    if (search != -1) {
        CHECK(lstrcmpA(narrow_found.name, "MOVED.BIN") == 0 && narrow_found.size == 8);
        CHECK(_findnext32(search, &narrow_found) == -1 && errno == ENOENT);
        CHECK(_findclose(search) == 0);
    }
    CHECK(_findfirst32("ABSENT.*", &narrow_found) == -1 && errno == ENOENT);
    CHECK(_findfirst32(NULL, &narrow_found) == -1 && errno == EINVAL);
    CHECK(_findnext32(-1, NULL) == -1 && errno == EINVAL);
    CHECK(_stati64("SMALL.BIN", &info) == -1 && errno == ENOENT);
    CHECK(_wunlink(L"MOVED.BIN") == 0);
    CHECK(_wremove(L"MOVED.BIN") == -1 && errno == ENOENT);

    stream = fopen("WIDE1.TXT", "wb");
    CHECK(stream != NULL);
    if (stream) { CHECK(fputws(message, stream) >= 0); CHECK(fclose(stream) == 0); }
    stream = fopen("WIDE2.TXT", "wb");
    CHECK(stream != NULL);
    if (stream) {
        for (i = 0; message[i]; ++i) CHECK(fputwc(message[i], stream) != WEOF);
        CHECK(fclose(stream) == 0);
    }
    fd = _open("WIDE1.TXT", _O_RDONLY | _O_BINARY);
    other = _open("WIDE2.TXT", _O_RDONLY | _O_BINARY);
    CHECK(fd >= 0 && other >= 0);
    if (fd >= 0 && other >= 0) {
        int a = _read(fd, bytes, 64), b = _read(other, reference, 64);
        BOOL equal = a > 0 && a == b;
        if (equal) for (i = 0; i < (unsigned)a; ++i) if (bytes[i] != reference[i]) equal = FALSE;
        CHECK(equal);
    }
    if (fd >= 0) _close(fd);
    if (other >= 0) _close(other);
    CHECK(_wremove(L"WIDE1.TXT") == 0);
    CHECK(_wremove(L"WIDE2.TXT") == 0);
    CHECK(RemoveDirectoryA("SUBDIR"));
    CHECK(SetCurrentDirectoryA(original));
    CHECK(RemoveDirectoryA(folder));

    lstrcatA(module, "CRTEST.TXT");
    output = CreateFileA(module, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) ExitProcess(7);
    wsprintfA(text, "RPCRT legacy runtime test\r\nCHECKS=%u FAILURES=%u FIRST=%u\r\n"
              "FSTAT=%d ERRNO=%d MODE=%x SIZE=%u:%u\r\nWFOPEN_ERRNO=%d WSOPEN_FD=%d WSOPEN_ERRNO=%d\r\nRESULT=%s\r\n",
              checks, failures, first_failure, fstat_status, fstat_error,
              fstat_mode, fstat_high, fstat_low, wide_open_error, native_fd, native_open_error, failures ? "FAIL" : "PASS");
    size = (DWORD)lstrlenA(text);
    if (!WriteFile(output, text, size, &got, NULL) || got != size) ExitProcess(8);
    CloseHandle(output);
    ExitProcess(failures ? 1 : 0);
}
