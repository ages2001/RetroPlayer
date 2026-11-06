/* RPUNIN.EXE - uninstaller for RetroPlayer (32-bit).
 *
 * Registered as the Add/Remove Programs UninstallString on Windows 95 and
 * Windows NT 4.0 and later. It removes the whole install folder and the
 * Uninstall registry key. ANSI APIs only (file and registry names are ASCII
 * 8.3), so one code path serves both 9x and NT. CRT-free: entry _RPEntry@0.
 *
 * Two stages. Stage 1 runs from the install folder, asks for confirmation,
 * copies itself to the TEMP folder and relaunches that copy with "/U:<dir>"
 * (a running EXE cannot delete its own folder). Stage 2 runs from TEMP, wipes
 * the folder and the registry key, then schedules the TEMP copy for deletion.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>       /* remove the NT 3.x Program Manager group */

#define UNINSTALL_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RetroPlayer"

static BOOL nt_system;

static void zero_mem(void *p, unsigned n) { unsigned char *c = p; while (n--) *c++ = 0; }

/* Minimal substring search (no CRT). */
static const char *find_sub(const char *hay, const char *needle)
{
    const char *h, *n;
    for (; *hay; ++hay) {
        for (h = hay, n = needle; *n && *h == *n; ++h, ++n) {}
        if (!*n) return hay;
    }
    return NULL;
}

/* Recursively delete a directory and everything under it. */
static void rmtree(const char *dir)
{
    char pat[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    wsprintfA(pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!lstrcmpA(fd.cFileName, ".") || !lstrcmpA(fd.cFileName, "..")) continue;
            wsprintfA(path, "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                rmtree(path);
            } else {
                SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(path);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

/* Drop the TEMP copy once this process exits: delay-until-reboot on NT, a
 * small self-deleting batch on 9x. A leftover is harmless either way. */
typedef BOOL (WINAPI *MOVEFILEEX_A)(LPCSTR, LPCSTR, DWORD);

static void drop_self(const char *self)
{
    char temp[MAX_PATH], bat[MAX_PATH], line[MAX_PATH * 3], cmd[MAX_PATH * 2];
    HANDLE f; DWORD w;
    /* MoveFileExA is not exported by every Windows 95 loader, so it is resolved
     * at run time rather than imported statically (a missing import would stop
     * RPUNIN from starting on 9x). */
    if (nt_system) {
        HMODULE k = GetModuleHandleA("kernel32.dll");
        MOVEFILEEX_A mv = k ? (MOVEFILEEX_A)GetProcAddress(k, "MoveFileExA") : NULL;
        if (mv) { mv(self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT); return; }
    }
    if (!GetTempPathA(MAX_PATH, temp)) return;
    wsprintfA(bat, "%sRPUN.BAT", temp);
    f = CreateFileA(bat, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    wsprintfA(line,
        "@echo off\r\n:r\r\ndel \"%s\"\r\nif exist \"%s\" goto r\r\ndel \"%s\"\r\n",
        self, self, bat);
    WriteFile(f, line, lstrlenA(line), &w, NULL);
    CloseHandle(f);
    wsprintfA(cmd, "%s /c \"%s\"", nt_system ? "cmd.exe" : "command.com", bat);
    WinExec(cmd, SW_HIDE);
}

static void remove_key(void)
{
    /* RegDeleteKeyA removes the leaf key (it has no subkeys of its own). */
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, UNINSTALL_KEY);
}

/* Delete the desktop shortcut created on 9x / NT 4+ (no-op if absent). */
static void remove_desktop_shortcut(void)
{
    HKEY key; DWORD type, size = MAX_PATH; char desktop[MAX_PATH], lnk[MAX_PATH];
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    if (RegQueryValueExA(key, "Desktop", NULL, &type, (BYTE*)desktop, &size) == ERROR_SUCCESS && desktop[0]) {
        wsprintfA(lnk, "%s\\RetroPlayer.lnk", desktop);
        SetFileAttributesA(lnk, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(lnk);
    }
    RegCloseKey(key);
}

/* Remove the Program Manager group created on NT 3.x. A no-op on shells that
 * do not answer the PROGMAN DDE conversation (9x/NT4 Explorer accept it too). */
static HDDEDATA CALLBACK dde_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2,
                                HDDEDATA d, ULONG_PTR a, ULONG_PTR b)
{
    (void)t; (void)f; (void)c; (void)h1; (void)h2; (void)d; (void)a; (void)b;
    return (HDDEDATA)0;
}
static void remove_pm_group(void)
{
    DWORD inst = 0; HSZ service, topic; HCONV conv;
    if (DdeInitializeA(&inst, dde_cb, APPCMD_CLIENTONLY, 0) != DMLERR_NO_ERROR) return;
    service = DdeCreateStringHandleA(inst, "PROGMAN", CP_WINANSI);
    topic   = DdeCreateStringHandleA(inst, "PROGMAN", CP_WINANSI);
    conv = DdeConnect(inst, service, topic, NULL);
    DdeFreeStringHandle(inst, service);
    DdeFreeStringHandle(inst, topic);
    if (conv) {
        DdeClientTransaction((LPBYTE)"[DeleteGroup(RetroPlayer)]", 27, conv, 0, 0,
                             XTYP_EXECUTE, 3000, NULL);
        DdeDisconnect(conv);
    }
    DdeUninitialize(inst);
}

void WINAPI RPEntry(void)
{
    char self[MAX_PATH], dir[MAX_PATH], temp[MAX_PATH], tself[MAX_PATH], cl[MAX_PATH * 2];
    const char *mark;
    DWORD n;

    nt_system = !(GetVersion() & 0x80000000UL);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    n = GetModuleFileNameA(NULL, self, MAX_PATH);
    if (!n || n >= MAX_PATH) ExitProcess(2);

    mark = find_sub(GetCommandLineA(), "/U:");
    if (!mark) {
        /* Stage 1: confirm, copy to TEMP, relaunch the copy, then quit. */
        STARTUPINFOA si; PROCESS_INFORMATION pi; char *p, *last = self;
        for (p = self; *p; ++p) if (*p == '\\') last = p;
        { unsigned i = 0; for (p = self; p < last; ++p) dir[i++] = *p; dir[i] = 0; }
        if (MessageBoxA(NULL, "Remove RetroPlayer and all of its files?",
                        "RetroPlayer Uninstall", MB_YESNO | MB_ICONQUESTION) != IDYES)
            ExitProcess(0);
        if (!GetTempPathA(MAX_PATH, temp)) ExitProcess(3);
        wsprintfA(tself, "%sRPUNIN.EXE", temp);
        if (!CopyFileA(self, tself, FALSE)) {
            /* Can't stage a copy: delete what we can directly instead. */
            rmtree(dir); remove_key();
            MessageBoxA(NULL, "RetroPlayer was removed.", "RetroPlayer Uninstall",
                        MB_OK | MB_ICONINFORMATION);
            ExitProcess(0);
        }
        wsprintfA(cl, "\"%s\" /U:%s", tself, dir);
        zero_mem(&si, sizeof(si)); si.cb = sizeof(si);
        if (CreateProcessA(tself, cl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
        ExitProcess(0);
    }

    /* Stage 2: running from TEMP. The install folder follows "/U:". */
    lstrcpyA(dir, mark + 3);
    { int i = lstrlenA(dir); while (i > 0 && (dir[i-1] == ' ' || dir[i-1] == '\r' ||
        dir[i-1] == '\n' || dir[i-1] == '"')) dir[--i] = 0; }
    rmtree(dir);
    remove_key();
    remove_desktop_shortcut();
    remove_pm_group();
    MessageBoxA(NULL, "RetroPlayer was removed.", "RetroPlayer Uninstall",
                MB_OK | MB_ICONINFORMATION);
    drop_self(self);
    ExitProcess(0);
}
