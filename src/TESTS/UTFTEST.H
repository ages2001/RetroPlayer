#include "UTFDATA.H"

static void test_utf8(void)
{
    WCHAR wide[64];
    char encoded[64];
    unsigned i, j;
    int result;
    BOOL equal;
    static const WCHAR broken[] = {0xd800, L'A', 0xdc00};
    static const unsigned char replacement[] = {0xef,0xbf,0xbd,'A',0xef,0xbf,0xbd};
    for (i = 0; i < sizeof(utf_cases)/sizeof(utf_cases[0]); ++i) {
        const UTFCASE *v = &utf_cases[i];
        CHECK(MultiByteToWideChar(CP_UTF8, 0, (LPCCH)v->input, v->bytes, NULL, 0) == v->units);
        result = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)v->input, v->bytes, wide, 64);
        CHECK(result == v->units);
        equal = TRUE;
        for (j = 0; j < (unsigned)v->units; ++j) if (wide[j] != v->wide[j]) equal = FALSE;
        CHECK(equal);
        wide[0] = 0x1234;
        result = MultiByteToWideChar(CP_UTF8, 8, (LPCCH)v->input, v->bytes, wide, 64);
        CHECK(v->valid ? result == v->units : result == 0 && GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
        if (!v->valid) CHECK(wide[0] == 0x1234);
        if (v->units > 1) {
            wide[0] = 0x1234;
            CHECK(MultiByteToWideChar(CP_UTF8, 0, (LPCCH)v->input, v->bytes, wide, v->units-1) == 0);
            CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER && wide[0] == 0x1234);
        }
        CHECK(WideCharToMultiByte(CP_UTF8, 0, v->wide, v->units, NULL, 0, NULL, NULL) == v->encoded);
        result = WideCharToMultiByte(CP_UTF8, 0, v->wide, v->units, encoded, 64, NULL, NULL);
        CHECK(result == v->encoded);
        equal = TRUE;
        for (j = 0; j < (unsigned)v->encoded; ++j)
            if ((unsigned char)encoded[j] != v->canonical[j]) equal = FALSE;
        CHECK(equal);
        if (v->encoded > 1) {
            encoded[0] = 0x55;
            CHECK(WideCharToMultiByte(CP_UTF8, 0, v->wide, v->units, encoded, v->encoded-1, NULL, NULL) == 0);
            CHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER && encoded[0] == 0x55);
        }
    }
    CHECK(MultiByteToWideChar(CP_UTF8, 0, "A\0B", -1, wide, 64) == 2 && wide[0] == 'A' && wide[1] == 0);
    CHECK(WideCharToMultiByte(CP_UTF8, 0, L"A\0B", -1, encoded, 64, NULL, NULL) == 2 && encoded[1] == 0);
    CHECK(MultiByteToWideChar(CP_UTF8, 4, "a", 1, wide, 64) == 0 && GetLastError() == ERROR_INVALID_FLAGS);
    CHECK(MultiByteToWideChar(CP_UTF8, 0, "a", 0, wide, 64) == 0 && GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(MultiByteToWideChar(CP_UTF8, 0, "a", 1, NULL, 1) == 0 && GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(WideCharToMultiByte(CP_UTF8, 0, broken, 3, encoded, 64, NULL, NULL) == 7);
    equal = TRUE;
    for (j = 0; j < 7; ++j) if ((unsigned char)encoded[j] != replacement[j]) equal = FALSE;
    CHECK(equal);
    encoded[0] = 0x55;
    CHECK(WideCharToMultiByte(CP_UTF8, 0x80, broken, 3, encoded, 64, NULL, NULL) == 0 &&
          GetLastError() == ERROR_NO_UNICODE_TRANSLATION && encoded[0] == 0x55);
    CHECK(WideCharToMultiByte(CP_UTF8, 0, L"A", 1, encoded, 64, "?", NULL) == 0 &&
          GetLastError() == ERROR_INVALID_PARAMETER);
}

static void test_wide_io(void)
{
    char path[MAX_PATH], magic[2];
    WCHAR wide[MAX_PATH];
    DWORD size;
    HANDLE file, mutex, semaphore;
    HMODULE module;
    CHECK(GetModuleFileNameA(NULL, path, MAX_PATH) != 0);
    CHECK(MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH) != 0);
    {
        WCHAR module_name[MAX_PATH];
        DWORD count = GetModuleFileNameW(NULL, module_name, MAX_PATH);
        unsigned i;
        BOOL equal = count > 0 && count < MAX_PATH;
        if (equal) for (i = 0; i <= count; ++i) if (module_name[i] != wide[i]) equal = FALSE;
        CHECK(equal);
        CHECK(GetFileAttributesW(wide) == GetFileAttributesA(path));
    }
    file = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL);
    CHECK(file != INVALID_HANDLE_VALUE && file != NULL);
    if (file != INVALID_HANDLE_VALUE && file != NULL) {
        CHECK(ReadFile(file, magic, 2, &size, NULL) && size == 2 && magic[0] == 'M' && magic[1] == 'Z');
        CloseHandle(file);
    }
    mutex = CreateMutexW(NULL, FALSE, NULL);
    CHECK(mutex != NULL);
    if (mutex) {
        CHECK(WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0);
        CHECK(ReleaseMutex(mutex)); CloseHandle(mutex);
    }
    semaphore = CreateSemaphoreW(NULL, 0, 2, NULL);
    CHECK(semaphore != NULL);
    if (semaphore) {
        CHECK(WaitForSingleObject(semaphore, 0) == WAIT_TIMEOUT);
        CHECK(ReleaseSemaphore(semaphore, 1, NULL));
        CHECK(WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0);
        CloseHandle(semaphore);
    }
    CHECK(GetModuleHandleW(L"KERNEL32.DLL") == GetModuleHandleA("KERNEL32.DLL"));
    CHECK(GetModuleHandleW(NULL) == GetModuleHandleA(NULL));
    module = LoadLibraryW(L"USER32.DLL");
    CHECK(module != NULL);
    if (module) FreeLibrary(module);
    module = LoadLibraryExW(L"USER32.DLL", NULL, 0);
    CHECK(module != NULL);
    if (module) FreeLibrary(module);
}
