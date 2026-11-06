#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char directory[MAX_PATH], command[MAX_PATH+8], path[MAX_PATH];
static STARTUPINFOA startup;
static PROCESS_INFORMATION process;
void WINAPI RPEntry(void)
{
    char *p, *last, text[256];
    DWORD count, error = 0, result = 0xffffffffUL, wait = 0xffffffffUL;
    DWORD atom_error = 0, worker_error = 0;
    HMODULE module;
    HANDLE output;
    count = GetModuleFileNameA(NULL, directory, MAX_PATH);
    if (!count || count >= MAX_PATH-16) ExitProcess(2);
    last = directory;
    for (p = directory; *p; ++p) if (*p == '\\') last = p+1;
    *last = 0;
    lstrcpyA(path, directory); lstrcatA(path, "RPATOM.DLL");
    module = LoadLibraryA(path);
    if (!module) atom_error = GetLastError();
    else FreeLibrary(module);
    lstrcpyA(path, directory); lstrcatA(path, "ATWORK.DLL");
    module = LoadLibraryA(path);
    if (!module) worker_error = GetLastError();
    else FreeLibrary(module);
    lstrcpyA(path, directory); lstrcatA(path, "ATTEST.TXT");
    DeleteFileA(path);
    lstrcpyA(path, directory); lstrcatA(path, "ATTEST.EXE");
    lstrcpyA(command, "\""); lstrcatA(command, path); lstrcatA(command, "\"");
    startup.cb = sizeof(startup);
    if (CreateProcessA(path, command, NULL, NULL, FALSE, 0, NULL, directory, &startup, &process)) {
        CloseHandle(process.hThread);
        wait = WaitForSingleObject(process.hProcess, 90000);
        if (!GetExitCodeProcess(process.hProcess, &result)) error = GetLastError();
        CloseHandle(process.hProcess);
    } else error = GetLastError();
    lstrcpyA(path, directory); lstrcatA(path, "RUNTEST.TXT");
    output = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) ExitProcess(3);
    wsprintfA(text, "Atomic process runner\r\nWAIT=%lu EXIT=%lu ERROR=%lu\r\nATOM_ERROR=%lu WORKER_ERROR=%lu\r\n",wait,result,error,atom_error,worker_error);
    if (!WriteFile(output,text,(DWORD)lstrlenA(text),&count,NULL)) ExitProcess(4);
    CloseHandle(output);
    ExitProcess((error || wait || result || atom_error || worker_error) ? 1 : 0);
}
