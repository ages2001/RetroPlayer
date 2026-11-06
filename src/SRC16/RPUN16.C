/* RPUN16.EXE - 16-bit uninstaller for RetroPlayer 16 (Windows 3.0/3.1).
 *
 * Added to Program Manager by RPSET16 with a red-X icon. It removes the install
 * folder (default C:\RETROPLY) and the "RetroPlayer" Program Manager group.
 * On Windows 3.x a running program's own file can be deleted, so no relaunch
 * trick is needed. DDEML is loaded at run time (Windows 3.0 may lack it).
 */
#include <windows.h>
#include <direct.h>      /* rmdir */
#include <dos.h>         /* _dos_findfirst / _dos_findnext */
#include <ddeml.h>

static char g_dir[160];        /* the install folder (this program's folder) */
static char g_selfname[16];    /* this program's file name, e.g. RPUN16.EXE  */

/* --- Program Manager group removal (best-effort, DDEML loaded at run time) -- */
typedef UINT     (WINAPI *P_DdeInitialize)(LPDWORD, PFNCALLBACK, DWORD, DWORD);
typedef HSZ      (WINAPI *P_DdeCreateStringHandle)(DWORD, LPSTR, int);
typedef HCONV    (WINAPI *P_DdeConnect)(DWORD, HSZ, HSZ, PCONVCONTEXT);
typedef HDDEDATA (WINAPI *P_DdeClientTransaction)(LPVOID, DWORD, HCONV, HSZ, UINT, UINT, DWORD, LPDWORD);
typedef BOOL     (WINAPI *P_DdeDisconnect)(HCONV);
typedef BOOL     (WINAPI *P_DdeUninitialize)(DWORD);
typedef BOOL     (WINAPI *P_DdeFreeStringHandle)(DWORD, HSZ);

HDDEDATA CALLBACK _export dde_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2,
                                        HDDEDATA d, DWORD a, DWORD b)
{
    (void)t; (void)f; (void)c; (void)h1; (void)h2; (void)d; (void)a; (void)b;
    return (HDDEDATA)0;
}

static void remove_pm_group(void)
{
    HINSTANCE dll;
    P_DdeInitialize pInit; P_DdeCreateStringHandle pStr; P_DdeConnect pConn;
    P_DdeClientTransaction pTx; P_DdeDisconnect pDisc; P_DdeUninitialize pUninit;
    P_DdeFreeStringHandle pFree;
    DWORD inst = 0; HSZ s, t; HCONV conv; char cmd[48];

    dll = LoadLibrary("DDEML.DLL");
    if ((UINT)dll < 32) return;
    pInit   = (P_DdeInitialize)GetProcAddress(dll, "DdeInitialize");
    pStr    = (P_DdeCreateStringHandle)GetProcAddress(dll, "DdeCreateStringHandle");
    pConn   = (P_DdeConnect)GetProcAddress(dll, "DdeConnect");
    pTx     = (P_DdeClientTransaction)GetProcAddress(dll, "DdeClientTransaction");
    pDisc   = (P_DdeDisconnect)GetProcAddress(dll, "DdeDisconnect");
    pUninit = (P_DdeUninitialize)GetProcAddress(dll, "DdeUninitialize");
    pFree   = (P_DdeFreeStringHandle)GetProcAddress(dll, "DdeFreeStringHandle");
    if (!pInit || !pStr || !pConn || !pTx || !pDisc || !pUninit || !pFree) {
        FreeLibrary(dll); return;
    }
    if (pInit(&inst, dde_cb, APPCMD_CLIENTONLY, 0) != DMLERR_NO_ERROR) { FreeLibrary(dll); return; }
    s = pStr(inst, "PROGMAN", CP_WINANSI);
    t = pStr(inst, "PROGMAN", CP_WINANSI);
    conv = pConn(inst, s, t, NULL);
    pFree(inst, s); pFree(inst, t);
    if (conv) {
        lstrcpy(cmd, "[DeleteGroup(RetroPlayer)]");
        pTx((LPVOID)cmd, lstrlen(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
        pDisc(conv);
    }
    pUninit(inst);
    FreeLibrary(dll);
}

/* --- file removal ---------------------------------------------------------- */
static void del_one(LPCSTR path)
{
    OFSTRUCT of;
    OpenFile(path, &of, OF_DELETE);
}

/* Delete every file in the folder except this program, then this program and
 * the folder itself (last, so no code runs after the self-delete). */
static void wipe_dir(const char *self)
{
    struct find_t fb;
    char pat[200], path[200];
    int r;
    wsprintf(pat, "%s\\*.*", (LPSTR)g_dir);
    r = _dos_findfirst(pat, _A_NORMAL | _A_RDONLY | _A_HIDDEN | _A_SYSTEM, &fb);
    while (r == 0) {
        if (lstrcmpi(fb.name, g_selfname) != 0) {
            wsprintf(path, "%s\\%s", (LPSTR)g_dir, (LPSTR)fb.name);
            del_one(path);
        }
        r = _dos_findnext(&fb);
    }
    del_one(self);          /* this program (allowed while running on Win16) */
    rmdir(g_dir);
}

int PASCAL WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmd, int show)
{
    char self[160], *p, *last;
    int i;

    (void)prev; (void)cmd; (void)show;
    if (!GetModuleFileName(hinst, self, sizeof(self))) return 1;
    last = self;
    for (p = self; *p; ++p) if (*p == '\\') last = p;
    for (i = 0, p = self; p < last; ++p) g_dir[i++] = *p;
    g_dir[i] = 0;
    lstrcpy(g_selfname, last + 1);

    if (MessageBox(0, "Remove RetroPlayer and all of its files?",
                   "RetroPlayer Uninstall", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;

    remove_pm_group();
    MessageBox(0, "RetroPlayer was removed.", "RetroPlayer Uninstall",
               MB_OK | MB_ICONINFORMATION);
    wipe_dir(self);
    return 0;
}
