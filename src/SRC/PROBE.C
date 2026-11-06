/* Runtime evidence for the engine port. Export presence is NOT acceleration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE output;
static BOOL failed;

static void line(const char *s)
{
    DWORD n, len = (DWORD)lstrlenA(s);
    if (!WriteFile(output, s, len, &n, NULL) || n != len) failed = TRUE;
}

static void library(const char *name, const char *const *symbols)
{
    HMODULE module = LoadLibraryA(name);
    char text[256];
    unsigned i;
    if (!module) {
        wsprintfA(text, "%s: unavailable (error %lu)\r\n", name, GetLastError());
        line(text);
        return;
    }
    wsprintfA(text, "%s: loadable\r\n", name); line(text);
    if (symbols) for (i = 0; symbols[i]; ++i) {
        wsprintfA(text, "  %s: %s\r\n", symbols[i],
                  GetProcAddress(module, symbols[i]) ? "export present" : "missing");
        line(text);
    }
    FreeLibrary(module);
}

void WINAPI RPEntry(void)
{
    static const char *const kernel[] = {
        "TryEnterCriticalSection", "InitializeCriticalSectionAndSpinCount",
        "GetSystemTimeAsFileTime", "GetFileSizeEx", "CancelIo", "CreateFileW",
        "LoadLibraryExW", "GetTickCount64", "GetNativeSystemInfo", NULL };
    static const char *const crt[] = { "_beginthreadex", "_fstat64", "_wopen", "_wfopen", NULL };
    static const char *const mm[] = { "waveOutOpen", "waveOutGetDevCapsA", "waveOutGetDevCapsW", NULL };
    static const char *const gdi[] = { "CreateDIBSection", "StretchDIBits", "BitBlt", NULL };
    char path[MAX_PATH], text[256], *last, *p, magic[2];
    WCHAR wide[MAX_PATH];
    DWORD version = GetVersion(), count;
    typedef HANDLE (WINAPI *CREATEW)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
    CREATEW createw;
    HANDLE file;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    count = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (!count || count >= MAX_PATH-1) ExitProcess(1);
    if (!MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH)) ExitProcess(1);
    last = path;
    for (p = path; *p; ++p) if (*p == '\\') last = p+1;
    if ((last-path)+9 >= MAX_PATH) ExitProcess(1);
    lstrcpyA(last, "CAPS.TXT");
    output = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) ExitProcess(1);
    wsprintfA(text, "RetroPlayer runtime capability probe\r\nOS=%u.%u family=%s\r\n",
              LOBYTE(LOWORD(version)), HIBYTE(LOWORD(version)),
              (version & 0x80000000UL) ? "9x" : "NT");
    line(text);
    library("KERNEL32.DLL", kernel);
    library("MSVCRT.DLL", crt);
    library("CRTDLL.DLL", crt);
    library("WINMM.DLL", mm);
    library("GDI32.DLL", gdi);
    library("DDRAW.DLL", NULL);
    library("D3D9.DLL", NULL);
    library("DXVA2.DLL", NULL);
    library("D3D11.DLL", NULL);
    createw = (CREATEW)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "CreateFileW");
    if (createw) {
        file = createw(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE || !file) {
            wsprintfA(text, "CreateFileW(self): FAILED error=%lu\r\n", GetLastError()); line(text);
        } else {
            if (ReadFile(file, magic, 2, &count, NULL) && count == 2 && magic[0] == 'M' && magic[1] == 'Z')
                line("CreateFileW + ReadFile(self MZ): PASS\r\n");
            else {
                wsprintfA(text, "CreateFileW + ReadFile(self MZ): FAILED error=%lu\r\n", GetLastError());
                line(text);
            }
            CloseHandle(file);
        }
    }
    line("DLL/export presence does not verify device support or playback.\r\n");
    if (!CloseHandle(output)) failed = TRUE;
    ExitProcess(failed ? 1 : 0);
}
