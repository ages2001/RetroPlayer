#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
__declspec(dllimport) unsigned __cdecl tls_work(unsigned, unsigned);
static __thread unsigned value = 17;
static __thread unsigned char aligned[127] __attribute__((aligned(4096)));
static HANDLE gate;
static unsigned checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) ++failures; } while (0)

static DWORD WINAPI worker(void *argument)
{
    unsigned id = (unsigned)(uintptr_t)argument, i, errors = 0;
    DWORD saved = 0x12340000UL + id;
    SetLastError(saved);
    if (value != 17 || GetLastError() != saved) ++errors;
    if ((uintptr_t)aligned & 4095) ++errors;
    for (i = 0; i < sizeof(aligned); ++i) if (aligned[i]) ++errors;
    if (!tls_work(23, id)) ++errors;
    value = id;
    aligned[0] = (unsigned char)id;
    if (WaitForSingleObject(gate, 5000) != WAIT_OBJECT_0) return errors + 1;
    for (i = 0; i < 100; ++i) {
        if (value != id || aligned[0] != (unsigned char)id) ++errors;
        if (!tls_work(id, id)) ++errors;
        Sleep(0);
    }
    return errors;
}

void WINAPI RPEntry(void)
{
    HANDLE threads[4], report;
    DWORD ids[4], code, wait, written, length;
    unsigned wave, i;
    char path[MAX_PATH], text[256], *p, *last;
    CHECK(value == 17 && tls_work(23, 999));
    value = 777;
    gate = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!gate) ExitProcess(2);
    for (wave = 0; wave < 32; ++wave) {
        ResetEvent(gate);
        for (i = 0; i < 4; ++i) {
            threads[i] = CreateThread(NULL, 0, worker, (void *)(uintptr_t)(wave*4+i+1), 0, &ids[i]);
            if (!threads[i]) ExitProcess(3);
        }
        SetEvent(gate);
        wait = WaitForMultipleObjects(4, threads, TRUE, 10000);
        CHECK(wait < WAIT_OBJECT_0 + 4);
        if (wait >= WAIT_OBJECT_0 + 4) ExitProcess(4);
        for (i = 0; i < 4; ++i) {
            CHECK(GetExitCodeThread(threads[i], &code) && code == 0);
            CloseHandle(threads[i]);
        }
    }
    CloseHandle(gate);
    CHECK(value == 777 && tls_work(999, 23));
    length = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH-16) ExitProcess(5);
    last = path;
    for (p = path; *p; ++p) if (*p == '\\') last = p+1;
    lstrcpyA(last,"TLTEST.TXT");
    report = CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (report == INVALID_HANDLE_VALUE) ExitProcess(6);
    wsprintfA(text,"GCC TLS compatibility test\r\nTHREADS=128 ALIGNMENT=4096\r\nCHECKS=%u FAILURES=%u\r\nRESULT=%s\r\n",
        checks,failures,failures ? "FAIL" : "PASS");
    length = (DWORD)lstrlenA(text);
    if (!WriteFile(report,text,length,&written,NULL) || written != length) ExitProcess(7);
    CloseHandle(report); ExitProcess(failures ? 1 : 0);
}
