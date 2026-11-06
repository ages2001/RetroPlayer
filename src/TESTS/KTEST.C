#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static CRITICAL_SECTION lock;
static HANDLE go, done;
static unsigned checks, failures, counter;
static unsigned first_failure;
static BOOL attempted;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures;if(!first_failure)first_failure=__LINE__; } } while (0)
#include "UTFTEST.H"

static void test_find(void)
{
    char folder[MAX_PATH],saved[MAX_PATH],*last;DWORD n,written;
    WIN32_FIND_DATAW data;WIN32_FIND_DATAA ansi;HANDLE handle;
    GetCurrentDirectoryA(MAX_PATH,saved);
    n=GetModuleFileNameA(NULL,folder,MAX_PATH);CHECK(n && n<MAX_PATH);
    last=folder;for(char *p=folder;*p;++p)if(*p=='\\')last=p+1;*last=0;
    CHECK(SetCurrentDirectoryA(folder));
    DeleteFileA("RFIND_A.BIN");DeleteFileA("RFIND_B.BIN");RemoveDirectoryA("RFIND_D");
    CHECK(CreateDirectoryA("RFIND_D",NULL));
    const char *names[]={"RFIND_A.BIN","RFIND_B.BIN"};
    for(unsigned i=0;i<2;++i) {
        handle=CreateFileA(names[i],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        CHECK(handle!=INVALID_HANDLE_VALUE);
        if(handle!=INVALID_HANDLE_VALUE) {
            CHECK(WriteFile(handle,"abc",3,&written,NULL) && written==3);CHECK(CloseHandle(handle));
        }
    }
    CHECK(AreFileApisANSI());
    SetFileApisToOEM();CHECK(!AreFileApisANSI());
    SetFileApisToANSI();CHECK(AreFileApisANSI());
    handle=FindFirstFileExW(L"RFIND_*",(FINDEX_INFO_LEVELS)1,&data,FindExSearchNameMatch,NULL,2);
    CHECK(handle!=INVALID_HANDLE_VALUE);
    if(handle!=INVALID_HANDLE_VALUE) {
        unsigned count=0,dirs=0;
        do {
            ++count;CHECK(data.cAlternateFileName[0]==0);
            if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)++dirs;
            else CHECK(data.nFileSizeHigh==0 && data.nFileSizeLow==3);
        } while(FindNextFileW(handle,&data));
        CHECK(GetLastError()==ERROR_NO_MORE_FILES);CHECK(count==3 && dirs==1);CHECK(FindClose(handle));
    }
    handle=FindFirstFileExW(L"RFIND_*",FindExInfoStandard,&data,FindExSearchLimitToDirectories,NULL,0);
    CHECK(handle!=INVALID_HANDLE_VALUE);
    if(handle!=INVALID_HANDLE_VALUE) {
        CHECK(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
        CHECK(!FindNextFileW(handle,&data) && GetLastError()==ERROR_NO_MORE_FILES);CHECK(FindClose(handle));
    }
    handle=FindFirstFileW(L"RFIND_?.BIN",&data);CHECK(handle!=INVALID_HANDLE_VALUE);
    if(handle!=INVALID_HANDLE_VALUE) {
        CHECK(FindNextFileW(handle,&data));CHECK(!FindNextFileW(handle,&data));CHECK(FindClose(handle));
    }
    handle=FindFirstFileA("RFIND_*",&ansi);CHECK(handle!=INVALID_HANDLE_VALUE);
    if(handle!=INVALID_HANDLE_VALUE)CHECK(FindClose(handle));
    char accented[32];
    CHECK(WideCharToMultiByte(CP_ACP,0,L"RFIND_\x00e9.BIN",-1,accented,sizeof(accented),NULL,NULL)>0);
    handle=CreateFileA(accented,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(handle!=INVALID_HANDLE_VALUE);if(handle!=INVALID_HANDLE_VALUE)CHECK(CloseHandle(handle));
    handle=FindFirstFileExW(L"RFIND_\x00e9.BIN",(FINDEX_INFO_LEVELS)1,&data,FindExSearchNameMatch,NULL,0);
    CHECK(handle!=INVALID_HANDLE_VALUE);
    if(handle!=INVALID_HANDLE_VALUE) {
        /* NT3.1 FAT canonicalizes its short names to uppercase. */
        CHECK((data.cFileName[6]==0xe9 || data.cFileName[6]==0xc9) && data.cAlternateFileName[0]==0);CHECK(FindClose(handle));
    }
    CHECK(DeleteFileA(accented));
    CHECK(FindFirstFileExW(L"RFIND_*",(FINDEX_INFO_LEVELS)99,&data,FindExSearchNameMatch,NULL,0)==INVALID_HANDLE_VALUE && GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(FindFirstFileExW(L"RFIND_*",FindExInfoStandard,&data,FindExSearchNameMatch,NULL,8)==INVALID_HANDLE_VALUE && GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(FindFirstFileExW(L"RFIND_*",FindExInfoStandard,&data,FindExSearchNameMatch,NULL,1)==INVALID_HANDLE_VALUE && GetLastError()==ERROR_NOT_SUPPORTED);
    CHECK(FindFirstFileExW(L"RFIND_*",FindExInfoStandard,NULL,FindExSearchNameMatch,NULL,0)==INVALID_HANDLE_VALUE && GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(DeleteFileA(names[0]));CHECK(DeleteFileA(names[1]));CHECK(RemoveDirectoryA("RFIND_D"));
    CHECK(SetCurrentDirectoryA(saved));
}

static void test_full_path(void)
{
    char expected[MAX_PATH], *part_a;
    WCHAR actual[MAX_PATH], converted[MAX_PATH], *part_w;
    DWORD length, count;
    unsigned i;
    length=GetFullPathNameA(".\\RETRO.TXT",MAX_PATH,expected,&part_a);
    CHECK(length && length<MAX_PATH && part_a);
    count=GetFullPathNameW(L".\\RETRO.TXT",0,NULL,NULL);
    CHECK(count>1 && count<MAX_PATH);
    actual[0]=0x1234;
    CHECK(GetFullPathNameW(L".\\RETRO.TXT",1,actual,NULL)==count);
    count=GetFullPathNameW(L".\\RETRO.TXT",MAX_PATH,actual,&part_w);
    CHECK(count>0 && count<MAX_PATH && part_w);
    CHECK(MultiByteToWideChar(CP_ACP,0,expected,-1,converted,MAX_PATH)>0);
    for (i=0; i<MAX_PATH && actual[i] && actual[i]==converted[i]; ++i) {}
    CHECK(i<MAX_PATH && actual[i]==converted[i]);
    CHECK(part_w && part_w[0]=='R' && part_w[8]=='T' && !part_w[9]);
    part_w=(WCHAR *)1;
    CHECK(GetFullPathNameW(L"C:\\",MAX_PATH,actual,&part_w)==3 && part_w==NULL);
}

static DWORD WINAPI TryWorker(void *unused)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < 3; ++i) {
        if (WaitForSingleObject(go, 10000) != WAIT_OBJECT_0) return 1;
        attempted = TryEnterCriticalSection(&lock);
        if (attempted) LeaveCriticalSection(&lock);
        SetEvent(done);
    }
    return 0;
}

static DWORD WINAPI CountWorker(void *unused)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < 10000; ++i) {
        EnterCriticalSection(&lock);
        ++counter;
        LeaveCriticalSection(&lock);
    }
    return 0;
}

void WINAPI RPEntry(void)
{
    HANDLE threads[4], thread, report;
    DWORD id, exitcode, wait, count, created = 0;
    unsigned i, j;
    SYSTEM_INFO original, compatible;
    DWORD_PTR process_mask, system_mask;
    FILETIME filetime, later;
    SYSTEMTIME utc;
    ULARGE_INTEGER first, last;
    CPINFO info;
    UINT pages[] = {CP_ACP, 437, 1252, 932, 0xffff};
    char path[MAX_PATH], text[256], *p, *end;
    {
        HMODULE found=NULL,expected=GetModuleHandleA("RPKERN.DLL");
        FARPROC address=GetProcAddress(expected,"GetModuleHandleExW");
        CHECK(GetModuleHandleExW(2,L"RPKERN.DLL",&found) && found==expected);
        CHECK(GetModuleHandleExW(6,(LPCWSTR)address,&found) && found==expected);
        CHECK(GetModuleHandleExW(4,(LPCWSTR)address,&found) && found==expected);
        if (found) CHECK(FreeLibrary(found));
        CHECK(!GetModuleHandleExW(8,L"RPKERN.DLL",&found) && GetLastError()==ERROR_INVALID_PARAMETER);
        CHECK(!GetModuleHandleExW(0,L"RPABSENT.DLL",&found) && found==NULL);
        CHECK(!GetModuleHandleExW(4,NULL,&found));
    }
    InitializeCriticalSection(&lock);
    EnterCriticalSection(&lock);
    CHECK(TryEnterCriticalSection(&lock)); /* recursive owner */
    go = CreateEventA(NULL, FALSE, FALSE, NULL);
    done = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(go && done);
    thread = go && done ? CreateThread(NULL, 0, TryWorker, NULL, 0, &id) : NULL;
    CHECK(thread != NULL);
    if (thread) {
        for (i = 0; i < 3; ++i) {
            SetEvent(go);
            wait = WaitForSingleObject(done, 10000);
            CHECK(wait == WAIT_OBJECT_0);
            if (wait != WAIT_OBJECT_0) ExitProcess(2);
            CHECK(attempted == (i == 2));
            if (i < 2) LeaveCriticalSection(&lock);
        }
        CHECK(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(thread, &exitcode) && exitcode == 0);
        CloseHandle(thread);
    } else {
        LeaveCriticalSection(&lock);
        LeaveCriticalSection(&lock);
    }
    if (go) CloseHandle(go);
    if (done) CloseHandle(done);
    for (i = 0; i < 4; ++i) {
        threads[i] = CreateThread(NULL, 0, CountWorker, NULL, 0, &id);
        CHECK(threads[i] != NULL);
        if (!threads[i]) break;
        ++created;
    }
    if (created) {
        wait = WaitForMultipleObjects(created, threads, TRUE, 60000);
        CHECK(wait - WAIT_OBJECT_0 < created);
        if (wait - WAIT_OBJECT_0 >= created) ExitProcess(3);
        for (i = 0; i < created; ++i) {
            CHECK(GetExitCodeThread(threads[i], &exitcode) && exitcode == 0);
            CloseHandle(threads[i]);
        }
    }
    CHECK(counter == 40000);
    DeleteCriticalSection(&lock);
    test_utf8();
    test_wide_io();
    test_full_path();
    test_find();
    GetSystemInfo(&original);
    GetNativeSystemInfo(&compatible);
    CHECK(original.dwPageSize == compatible.dwPageSize);
    CHECK(original.dwNumberOfProcessors == compatible.dwNumberOfProcessors);
    CHECK(original.dwProcessorType == compatible.dwProcessorType);
    CHECK(GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask));
    CHECK(process_mask && !(process_mask & ~system_mask));
    CHECK(system_mask==original.dwActiveProcessorMask);
    CHECK(!GetProcessAffinityMask(NULL,&process_mask,&system_mask));
    CHECK(!GetProcessAffinityMask(GetCurrentProcess(),NULL,&system_mask) && GetLastError()==ERROR_INVALID_PARAMETER);
    GetSystemTimeAsFileTime(&filetime);
    GetSystemTime(&utc);
    CHECK(SystemTimeToFileTime(&utc, &later));
    first.LowPart = filetime.dwLowDateTime; first.HighPart = filetime.dwHighDateTime;
    last.LowPart = later.dwLowDateTime; last.HighPart = later.dwHighDateTime;
    CHECK(last.QuadPart >= first.QuadPart && last.QuadPart - first.QuadPart < 20000000ULL);
    for (i = 0; i < sizeof(pages)/sizeof(pages[0]); ++i) {
        BOOL valid = GetCPInfo(pages[i], &info);
        for (j = 0; j < 256; ++j) {
            unsigned k;
            BOOL expected = FALSE;
            if (valid) for (k = 0; k + 1 < MAX_LEADBYTES && info.LeadByte[k]; k += 2)
                if (j >= info.LeadByte[k] && j <= info.LeadByte[k+1]) expected = TRUE;
            CHECK(!!IsDBCSLeadByteEx(pages[i], (BYTE)j) == !!expected);
        }
    }
    count = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (!count || count >= MAX_PATH - 12) ExitProcess(4);
    end = path;
    for (p = path; *p; ++p) if (*p == '\\') end = p+1;
    lstrcpyA(end, "KTEST.TXT");
    report = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (report == INVALID_HANDLE_VALUE) ExitProcess(5);
    wsprintfA(text, "RPKERN legacy API test\r\nCHECKS=%u FAILURES=%u COUNT=%u FIRST_FAIL=%u\r\nRESULT=%s\r\n",
              checks, failures, counter, first_failure, failures ? "FAIL" : "PASS");
    if (!WriteFile(report, text, (DWORD)lstrlenA(text), &count, NULL) || count != (DWORD)lstrlenA(text))
        ExitProcess(6);
    CloseHandle(report);
    ExitProcess(failures ? 1 : 0);
}
