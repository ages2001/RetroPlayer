/* Self-extracting Win32 installer for RetroPlayer (RPSET32.EXE).
 *
 * The player payload is appended to this executable:
 *   [entry]...[entry][payload_size: u32 LE]["RPSX": 4 bytes]
 * each entry is
 *   [name_len: u16 LE][name][data_len: u32 LE][crc32: u32 LE][data]
 * where name is a DOS 8.3 relative path (e.g. "RETROPLY.EXE",
 * "PLUGINS\\P1234567.DLL"). SCRIPTS/INSPACK.PY builds the payload and appends
 * it. No registry edits, file associations, drivers or OS files are touched. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>       /* Program Manager group creation on NT 3.x */
#include <objbase.h>     /* COM for the desktop shortcut (9x / NT 4+)     */
#include <shlobj.h>      /* IShellLink / IPersistFile                     */

#define RP_FILES 512
#define RP_NAME 80
static WCHAR target_dir[MAX_PATH], target_path[MAX_PATH];
static char written[RP_FILES][RP_NAME];
static unsigned written_count;
static DWORD crc_table[256];
static unsigned char io_buffer[16384];
static BOOL nt_system;
static HANDLE self;

/* --- basic helpers --------------------------------------------------------- */
static unsigned length(const WCHAR *s) { unsigned n = 0; while (s[n]) ++n; return n; }
static void copy_w(WCHAR *d, const WCHAR *s) { while ((*d++ = *s++)) {} }
static BOOL join(WCHAR *out, const WCHAR *base, const char *name)
{
    unsigned n = length(base), i = 0;
    if (n + 1 + (unsigned)lstrlenA(name) >= MAX_PATH) return FALSE;
    copy_w(out, base); out[n++] = '\\';
    while (name[i]) { out[n++] = (unsigned char)name[i++]; } out[n] = 0;
    return TRUE;
}
static BOOL ansi(const WCHAR *w, char *out)
{
    BOOL replaced = FALSE;
    return WideCharToMultiByte(CP_ACP, 0, w, -1, out, MAX_PATH, NULL, &replaced) > 0 && !replaced;
}
static HANDLE open_file(const WCHAR *path, DWORD access, DWORD share, DWORD action)
{
    char a[MAX_PATH];
    if (nt_system) return CreateFileW(path, access, share, NULL, action, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!ansi(path, a)) { SetLastError(ERROR_NO_UNICODE_TRANSLATION); return INVALID_HANDLE_VALUE; }
    return CreateFileA(a, access, share, NULL, action, FILE_ATTRIBUTE_NORMAL, NULL);
}
static DWORD attributes(const WCHAR *path)
{
    char a[MAX_PATH];
    if (nt_system) return GetFileAttributesW(path);
    if (!ansi(path, a)) return INVALID_FILE_ATTRIBUTES;
    return GetFileAttributesA(a);
}
static BOOL make_dir(const WCHAR *path)
{
    char a[MAX_PATH]; BOOL ok; DWORD at;
    if (nt_system) ok = CreateDirectoryW(path, NULL);
    else { if (!ansi(path, a)) return FALSE; ok = CreateDirectoryA(a, NULL); }
    if (ok) return TRUE;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    at = attributes(path);
    return at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
static BOOL remove_file(const WCHAR *path)
{
    char a[MAX_PATH];
    if (nt_system) { SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL); return DeleteFileW(path); }
    if (!ansi(path, a)) return FALSE;
    SetFileAttributesA(a, FILE_ATTRIBUTE_NORMAL); return DeleteFileA(a);
}
static BOOL parents(WCHAR *path)
{
    unsigned i;
    for (i = 3; path[i]; ++i) if (path[i] == '\\') {
        path[i] = 0; { BOOL ok = make_dir(path); path[i] = '\\'; if (!ok) return FALSE; }
    }
    return TRUE;
}
static BOOL safe_name(const char *path)
{
    unsigned base = 0, ext = 0, total = 0; BOOL dot = FALSE;
    char component[9]; unsigned c = 0;
    for (;; ++path) {
        char ch = *path;
        if (!ch || ch == '\\') {
            if (!base || (dot && !ext)) return FALSE;
            component[c] = 0;
            if (!lstrcmpA(component,"CON") || !lstrcmpA(component,"PRN") ||
                !lstrcmpA(component,"AUX") || !lstrcmpA(component,"NUL")) return FALSE;
            if (!ch) return TRUE;
            base = ext = c = 0; dot = FALSE;
        } else if (ch == '.' && !dot && base) dot = TRUE;
        else {
            if (!((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-')) return FALSE;
            if (dot) { if (++ext > 3) return FALSE; }
            else { if (++base > 8) return FALSE; component[c++] = ch; }
        }
        if (++total >= RP_NAME - 1) return FALSE;
    }
}
static void crc_init(void)
{
    unsigned i, b;
    for (i = 0; i < 256; ++i) { DWORD c = i; for (b = 0; b < 8; ++b) c = (c>>1)^((c&1)?0xEDB88320UL:0); crc_table[i] = c; }
}

/* Destination folder:
 *   NT 3.1 / 3.5x -> C:\RETROPLY
 *   9x / NT 4+    -> %ProgramFiles%\Retro Player  (Program Files (x86) on x64) */
static void compute_target(DWORD version, WCHAR *out)
{
    WCHAR pf[MAX_PATH]; const WCHAR *suffix = L"\\Retro Player"; unsigned len, i = 0; DWORD n = 0;
    if (!(version & 0x80000000UL) && LOBYTE(LOWORD(version)) < 4) { copy_w(out, L"C:\\RETROPLY"); return; }
    if (nt_system) n = GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
    else { char a[MAX_PATH]; DWORD m = GetEnvironmentVariableA("ProgramFiles", a, MAX_PATH);
           if (m && m < MAX_PATH && MultiByteToWideChar(CP_ACP,0,a,-1,pf,MAX_PATH)) n = length(pf); }
    if (!n || n >= MAX_PATH - 16) copy_w(pf, L"C:\\Program Files");
    len = length(pf); copy_w(out, pf);
    while (suffix[i] && len + 1 < MAX_PATH) out[len++] = suffix[i++];
    out[len] = 0;
}

/* --- wizard state ---------------------------------------------------------- */
#ifndef RP_INSTALL_TEST
static DWORD install_error;
static void zero_mem(void *p, unsigned n) { unsigned char *c = p; while (n--) *c++ = 0; }
static HINSTANCE app_inst;
static HWND wnd, ui_title, ui_body, ui_edit, ui_back, ui_next, ui_cancel;
static HFONT ui_font, ui_font_bold;
static int ui_page;          /* 0 welcome, 1 path, 2 confirm, 3 installing, 4 done, 5 error */
static int ui_cw, ui_ch;
static BOOL install_ok;
static void pump(void)
{
    MSG message;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageA(&message); }
}
static void progress(const char *name)
{
    char text[220]; wsprintfA(text, "Installing files, please wait...\r\n\r\n%s", name);
    if (ui_body) SetWindowTextA(ui_body, text);
    pump();
}
#else
static void pump(void) {}
static void progress(const char *name) { (void)name; }
#endif

/* --- payload extraction ---------------------------------------------------- */
static BOOL read_at(LONGLONG offset, void *buf, DWORD len)
{
    LONG high = (LONG)(offset >> 32); DWORD got;
    if (SetFilePointer(self, (LONG)offset, &high, FILE_BEGIN) == INVALID_SET_FILE_POINTER
        && GetLastError() != NO_ERROR) return FALSE;
    return ReadFile(self, buf, len, &got, NULL) && got == len;
}
static DWORD u16(const unsigned char *p) { return p[0] | (p[1]<<8); }
static DWORD u32(const unsigned char *p) { return p[0] | (p[1]<<8) | ((DWORD)p[2]<<16) | ((DWORD)p[3]<<24); }

static void undo(void)
{
    WCHAR path[MAX_PATH]; unsigned i;
    for (i = 0; i < written_count; ++i) if (join(path, target_dir, written[i])) remove_file(path);
}

static BOOL extract(LONGLONG start, LONGLONG end)
{
    LONGLONG pos = start;
    crc_init();
    written_count = 0;
    while (pos < end) {
        unsigned char hdr[2], lenbuf[8]; char name[RP_NAME]; DWORD nlen, dlen, want_crc, crc, chunk, left, wrote;
        HANDLE f;
        if (!read_at(pos, hdr, 2)) return FALSE;
        pos += 2;
        nlen = u16(hdr);
        if (nlen == 0 || nlen >= RP_NAME) return FALSE;
        if (!read_at(pos, name, nlen)) return FALSE;
        pos += nlen; name[nlen] = 0;
        if (!safe_name(name)) return FALSE;
        if (!read_at(pos, lenbuf, 8)) return FALSE;
        pos += 8;
        dlen = u32(lenbuf); want_crc = u32(lenbuf + 4);
        progress(name);
        if (!join(target_path, target_dir, name) || !parents(target_path)) return FALSE;
        f = open_file(target_path, GENERIC_WRITE, 0, CREATE_ALWAYS);
        if (f == INVALID_HANDLE_VALUE) return FALSE;
        if (written_count < RP_FILES) { lstrcpyA(written[written_count], name); ++written_count; }
        crc = 0xffffffffUL; left = dlen;
        while (left) {
            DWORD k;
            chunk = left < sizeof(io_buffer) ? left : (DWORD)sizeof(io_buffer);
            if (!read_at(pos, io_buffer, chunk)) { CloseHandle(f); return FALSE; }
            for (k = 0; k < chunk; ++k) crc = (crc>>8) ^ crc_table[(crc ^ io_buffer[k]) & 255];
            if (!WriteFile(f, io_buffer, chunk, &wrote, NULL) || wrote != chunk) { CloseHandle(f); return FALSE; }
            pos += chunk; left -= chunk;
            if (!(left & 0x3ffff)) pump();
        }
        CloseHandle(f);
        if ((crc ^ 0xffffffffUL) != want_crc) { SetLastError(ERROR_CRC); return FALSE; }
    }
    return TRUE;
}

#ifndef RP_INSTALL_TEST
/* --- wizard pages ---------------------------------------------------------- */
/* Control ids kept away from IDOK(1)/IDCANCEL(2) so IsDialogMessage never
 * routes a stray Enter/Escape to one of our buttons. */
#define RP_BACK   1001
#define RP_NEXT   1002
#define RP_CANCEL 1003
#define UNINSTALL_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RetroPlayer"
static LONGLONG payload_begin, payload_end;

static void set_btn(HWND b, const char *txt, BOOL show, BOOL enable)
{
    SetWindowTextA(b, txt);
    ShowWindow(b, show ? SW_SHOW : SW_HIDE);
    EnableWindow(b, enable);
}

static void show_page(int p)
{
    char a[MAX_PATH], body[900];
    ui_page = p;
    ansi(target_dir, a);
    ShowWindow(ui_edit, (p == 1) ? SW_SHOW : SW_HIDE);
    switch (p) {
    case 0:
        SetWindowTextA(ui_title, "Welcome to RetroPlayer Setup");
        SetWindowTextA(ui_body,
            "RetroPlayer version 1.0\r\nby ages2001\r\n\r\n"
            "This installs the RetroPlayer media player on your computer. It runs on "
            "Windows NT 3.1 and later and on Windows 95 and later, and plays audio and "
            "video with its own built-in engine.\r\n\r\n"
            "Close any running copy of RetroPlayer, then click Next to continue.");
        set_btn(ui_back, "< Back", TRUE, FALSE);
        set_btn(ui_next, "Next >", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        break;
    case 1:
        SetWindowTextA(ui_title, "Choose Install Location");
        SetWindowTextA(ui_body,
            "Setup will install RetroPlayer into the folder shown below.\r\n\r\n"
            "To use a different folder, edit the path. Then click Next to continue.");
        SetWindowTextA(ui_edit, a);
        set_btn(ui_back, "< Back", TRUE, TRUE);
        set_btn(ui_next, "Next >", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        SetFocus(ui_edit);
        break;
    case 2:
        SetWindowTextA(ui_title, "Ready to Install");
        wsprintfA(body,
            "RetroPlayer is ready to be installed into:\r\n\r\n    %s\r\n\r\n"
            "Make sure RetroPlayer is not running, then click Install to begin.", a);
        SetWindowTextA(ui_body, body);
        set_btn(ui_back, "< Back", TRUE, TRUE);
        set_btn(ui_next, "Install", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        break;
    case 3:
        SetWindowTextA(ui_title, "Installing");
        SetWindowTextA(ui_body, "Installing files, please wait...");
        set_btn(ui_back, "< Back", TRUE, FALSE);
        set_btn(ui_next, "Install", TRUE, FALSE);
        set_btn(ui_cancel, "Cancel", TRUE, FALSE);
        break;
    case 4:
        SetWindowTextA(ui_title, "Installation Complete");
        wsprintfA(body,
            "RetroPlayer was installed into:\r\n\r\n    %s\r\n\r\n"
            "Run RETROPLY.EXE from that folder to start the player.", a);
        SetWindowTextA(ui_body, body);
        set_btn(ui_back, "< Back", FALSE, FALSE);
        set_btn(ui_next, "Finish", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", FALSE, FALSE);
        break;
    default: /* 5 error */
        SetWindowTextA(ui_title, "Installation Failed");
        wsprintfA(body,
            "Installation failed (error %lu).\r\nNo files were left behind.", install_error);
        SetWindowTextA(ui_body, body);
        set_btn(ui_back, "< Back", FALSE, FALSE);
        set_btn(ui_next, "Close", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", FALSE, FALSE);
        break;
    }
}

/* Read the edit field into target_dir, trimming and sanity-checking the path. */
static BOOL capture_path(void)
{
    char a[MAX_PATH]; WCHAR w[MAX_PATH]; int n, s = 0;
    n = GetWindowTextA(ui_edit, a, MAX_PATH);
    while (n > 0 && (a[n-1] == ' ' || a[n-1] == '\t')) a[--n] = 0;
    while (n > 3 && a[n-1] == '\\') a[--n] = 0;           /* keep "C:\" intact */
    while (a[s] == ' ' || a[s] == '\t') ++s;
    if (s) { int i = 0; do { a[i] = a[s+i]; } while (a[i++]); n -= s; }
    if (n < 3 || a[1] != ':' || a[2] != '\\') {
        MessageBoxA(wnd, "Enter a full path such as C:\\Retro Player.",
                    "RetroPlayer Setup", MB_OK|MB_ICONEXCLAMATION);
        return FALSE;
    }
    if (!MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH)) return FALSE;
    copy_w(target_dir, w);
    return TRUE;
}

/* Register in Add/Remove Programs (Windows 95 and NT 4.0+). NT 3.x has no such
 * applet, so it is skipped there. The uninstaller RPUNIN.EXE ships in the
 * payload and is extracted into the install folder. ANSI registry calls work
 * on both 9x and NT. */
static void register_uninstall(DWORD version)
{
    HKEY key; char a[MAX_PATH], val[MAX_PATH]; DWORD one = 1, disp;
    if (nt_system && LOBYTE(LOWORD(version)) < 4) return;   /* no applet on NT 3.x */
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, NULL, 0,
                        KEY_WRITE, NULL, &key, &disp) != ERROR_SUCCESS) return;
    ansi(target_dir, a);
    wsprintfA(val, "%s\\RPUNIN.EXE", a);
    RegSetValueExA(key, "DisplayName",     0, REG_SZ, (const BYTE*)"RetroPlayer", 12);
    RegSetValueExA(key, "DisplayVersion",  0, REG_SZ, (const BYTE*)"1.0", 4);
    RegSetValueExA(key, "Publisher",       0, REG_SZ, (const BYTE*)"ages2001", 9);
    RegSetValueExA(key, "InstallLocation", 0, REG_SZ, (const BYTE*)a, lstrlenA(a) + 1);
    RegSetValueExA(key, "UninstallString", 0, REG_SZ, (const BYTE*)val, lstrlenA(val) + 1);
    RegSetValueExA(key, "NoModify", 0, REG_DWORD, (const BYTE*)&one, 4);
    RegSetValueExA(key, "NoRepair", 0, REG_DWORD, (const BYTE*)&one, 4);
    RegCloseKey(key);
}

/* Program Manager group with a RetroPlayer item and a red-X uninstaller item,
 * added over DDE on NT 3.x (which has no Add/Remove Programs applet). */
static DWORD dde_inst;
static HDDEDATA CALLBACK dde_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2,
                                HDDEDATA d, ULONG_PTR a, ULONG_PTR b)
{
    (void)t; (void)f; (void)c; (void)h1; (void)h2; (void)d; (void)a; (void)b;
    return (HDDEDATA)0;
}
static void dde_exec(HCONV conv, const char *cmd)
{
    DdeClientTransaction((LPBYTE)cmd, lstrlenA(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
}
static void create_pm_icons(void)
{
    HSZ service, topic; HCONV conv; char a[MAX_PATH], exe[MAX_PATH], uni[MAX_PATH], cmd[MAX_PATH + 64];
    dde_inst = 0;
    if (DdeInitializeA(&dde_inst, dde_cb, APPCMD_CLIENTONLY, 0) != DMLERR_NO_ERROR) return;
    service = DdeCreateStringHandleA(dde_inst, "PROGMAN", CP_WINANSI);
    topic   = DdeCreateStringHandleA(dde_inst, "PROGMAN", CP_WINANSI);
    conv = DdeConnect(dde_inst, service, topic, NULL);
    DdeFreeStringHandle(dde_inst, service);
    DdeFreeStringHandle(dde_inst, topic);
    if (!conv) { DdeUninitialize(dde_inst); return; }
    ansi(target_dir, a);
    wsprintfA(exe, "%s\\RETROPLY.EXE", a);
    wsprintfA(uni, "%s\\RPUNIN.EXE", a);
    dde_exec(conv, "[CreateGroup(RetroPlayer)]");
    dde_exec(conv, "[ShowGroup(RetroPlayer,1)]");
    wsprintfA(cmd, "[AddItem(%s,RetroPlayer,%s,0)]", exe, exe);
    dde_exec(conv, cmd);
    wsprintfA(cmd, "[AddItem(%s,Uninstall RetroPlayer,%s,0)]", uni, uni);
    dde_exec(conv, cmd);
    DdeDisconnect(conv);
    DdeUninitialize(dde_inst);
}

/* Desktop shortcut to the player (9x / NT 4+ Explorer shells). GUIDs are
 * defined locally so no import library is needed in the CRT-free build. */
static const GUID RP_CLSID_ShellLink = {0x00021401,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID RP_IID_IShellLinkA = {0x000214EE,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID RP_IID_IPersistFile= {0x0000010B,0,0,{0xC0,0,0,0,0,0,0,0x46}};

static BOOL desktop_folder(char *out)
{
    HKEY key; DWORD type, size = MAX_PATH; LONG r;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return FALSE;
    r = RegQueryValueExA(key, "Desktop", NULL, &type, (BYTE*)out, &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && out[0];
}

static void create_desktop_shortcut(void)
{
    char a[MAX_PATH], exe[MAX_PATH], desktop[MAX_PATH], lnk[MAX_PATH];
    WCHAR wlnk[MAX_PATH];
    IShellLinkA *psl; IPersistFile *ppf;
    if (!desktop_folder(desktop)) return;
    ansi(target_dir, a);
    wsprintfA(exe, "%s\\RETROPLY.EXE", a);
    wsprintfA(lnk, "%s\\RetroPlayer.lnk", desktop);
    CoInitialize(NULL);
    if (CoCreateInstance(&RP_CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                         &RP_IID_IShellLinkA, (void**)&psl) == S_OK) {
        psl->lpVtbl->SetPath(psl, exe);
        psl->lpVtbl->SetIconLocation(psl, exe, 0);
        psl->lpVtbl->SetWorkingDirectory(psl, a);
        if (psl->lpVtbl->QueryInterface(psl, &RP_IID_IPersistFile, (void**)&ppf) == S_OK) {
            if (MultiByteToWideChar(CP_ACP, 0, lnk, -1, wlnk, MAX_PATH))
                ppf->lpVtbl->Save(ppf, wlnk, TRUE);
            ppf->lpVtbl->Release(ppf);
        }
        psl->lpVtbl->Release(psl);
    }
    CoUninitialize();
}

static void do_install(void)
{
    DWORD version = GetVersion();
    show_page(3);
    UpdateWindow(wnd); pump();
    { WCHAR tp[MAX_PATH]; copy_w(tp, target_dir); parents(tp); }
    make_dir(target_dir);
    if (extract(payload_begin, payload_end)) {
        install_ok = TRUE;
        if (nt_system && LOBYTE(LOWORD(version)) < 4) {
            create_pm_icons();            /* NT 3.x: Program Manager player + red-X uninstaller */
        } else {
            register_uninstall(version);  /* 9x / NT 4+: Add/Remove Programs */
            create_desktop_shortcut();    /* 9x / NT 4+: desktop shortcut to the player */
        }
        if (self != INVALID_HANDLE_VALUE) { CloseHandle(self); self = INVALID_HANDLE_VALUE; }
        show_page(4);
    } else {
        install_error = GetLastError();
        undo();
        if (self != INVALID_HANDLE_VALUE) { CloseHandle(self); self = INVALID_HANDLE_VALUE; }
        show_page(5);
    }
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM a, LPARAM b)
{
    switch (m) {
    case WM_CREATE: {
        RECT rc; int by, bw = 96, bh = 28, gap = 8, cx, nx, bx;
        GetClientRect(h, &rc); ui_cw = rc.right; ui_ch = rc.bottom;
        ui_title = CreateWindowExA(0, "STATIC", "", WS_CHILD|WS_VISIBLE|SS_LEFT,
            24, 18, ui_cw-48, 26, h, NULL, app_inst, NULL);
        ui_body = CreateWindowExA(0, "STATIC", "", WS_CHILD|WS_VISIBLE|SS_LEFT,
            24, 54, ui_cw-48, 214, h, NULL, app_inst, NULL);
        ui_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL, 24, 150, ui_cw-48, 26, h, NULL, app_inst, NULL);
        by = ui_ch - 44;
        cx = ui_cw - 16 - bw; nx = cx - gap - bw; bx = nx - gap - bw;
        ui_back   = CreateWindowExA(0, "BUTTON", "< Back", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,    bx, by, bw, bh, h, (HMENU)RP_BACK,   app_inst, NULL);
        ui_next   = CreateWindowExA(0, "BUTTON", "Next >", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON, nx, by, bw, bh, h, (HMENU)RP_NEXT,   app_inst, NULL);
        ui_cancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,    cx, by, bw, bh, h, (HMENU)RP_CANCEL, app_inst, NULL);
        SendMessageA(ui_title,  WM_SETFONT, (WPARAM)ui_font_bold, 0);
        SendMessageA(ui_body,   WM_SETFONT, (WPARAM)ui_font, 0);
        SendMessageA(ui_edit,   WM_SETFONT, (WPARAM)ui_font, 0);
        SendMessageA(ui_back,   WM_SETFONT, (WPARAM)ui_font, 0);
        SendMessageA(ui_next,   WM_SETFONT, (WPARAM)ui_font, 0);
        SendMessageA(ui_cancel, WM_SETFONT, (WPARAM)ui_font, 0);
        show_page(0);
        SetFocus(ui_next);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(a)) {
        case RP_BACK:
            if (ui_page == 1) show_page(0);
            else if (ui_page == 2) show_page(1);
            return 0;
        case RP_NEXT:                                      /* Next / Install / Finish / Close */
            if (ui_page == 0) show_page(1);
            else if (ui_page == 1) { if (capture_path()) show_page(2); }
            else if (ui_page == 2) do_install();
            else if (ui_page >= 4) PostQuitMessage(0);
            return 0;
        case RP_CANCEL:
            if (ui_page < 3) PostQuitMessage(0);
            return 0;
        }
        return 0;
    case WM_CLOSE:
        if (ui_page != 3) PostQuitMessage(0);             /* no cancel during extraction */
        return 0;
    }
    return DefWindowProcA(h, m, a, b);
}

void WINAPI RPEntry(void)
{
    DWORD version = GetVersion(), size; WCHAR modulew[MAX_PATH]; char modulea[MAX_PATH];
    LONGLONG filesize, payload_size, start; unsigned char trailer[8];
    WNDCLASSA cls; MSG message;
    nt_system = !(version & 0x80000000UL);
    self = INVALID_HANDLE_VALUE;
    app_inst = GetModuleHandleA(NULL);
    if (!nt_system && LOBYTE(LOWORD(version)) < 4) {
        MessageBoxA(NULL, "This installer needs Windows NT 3.1 or later, or Windows 95 or later.",
                    "RetroPlayer Setup", MB_OK|MB_ICONEXCLAMATION); ExitProcess(77);
    }
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    size = nt_system ? GetModuleFileNameW(NULL, modulew, MAX_PATH) : GetModuleFileNameA(NULL, modulea, MAX_PATH);
    if (!size || size >= MAX_PATH) ExitProcess(2);
    if (nt_system) self = CreateFileW(modulew, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    else self = CreateFileA(modulea, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (self == INVALID_HANDLE_VALUE) goto invalid;
    { DWORD hi = 0, lo = GetFileSize(self, &hi); filesize = ((LONGLONG)hi << 32) | lo; }
    if (filesize < 8 || !read_at(filesize - 8, trailer, 8)) goto invalid;
    if (trailer[4] != 'R' || trailer[5] != 'P' || trailer[6] != 'S' || trailer[7] != 'X') goto invalid;
    payload_size = u32(trailer);
    if (payload_size <= 0 || payload_size > filesize - 8) goto invalid;
    start = filesize - 8 - payload_size;
    payload_begin = start; payload_end = filesize - 8;

    compute_target(version, target_dir);                  /* the default; user may edit it */

    ui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    ui_font_bold = CreateFontA(-19, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_SWISS, "MS Sans Serif");
    if (!ui_font_bold) ui_font_bold = ui_font;

    zero_mem(&cls, sizeof(cls));
    cls.lpfnWndProc = wnd_proc; cls.hInstance = app_inst;
    cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    cls.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    cls.lpszClassName = "RPSETUP";
    if (!RegisterClassA(&cls)) goto invalid;
    wnd = CreateWindowExA(0, "RPSETUP", "RetroPlayer Setup",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 544, 420, NULL, NULL, app_inst, NULL);
    if (!wnd) goto invalid;
    ShowWindow(wnd, SW_SHOW); UpdateWindow(wnd);

    while (GetMessageA(&message, NULL, 0, 0)) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN) {
            if (IsWindowEnabled(ui_next)) SendMessageA(wnd, WM_COMMAND, RP_NEXT, (LPARAM)ui_next);
            continue;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            if (IsWindowEnabled(ui_cancel)) SendMessageA(wnd, WM_COMMAND, RP_CANCEL, (LPARAM)ui_cancel);
            continue;
        }
        if (!IsDialogMessageA(wnd, &message)) { TranslateMessage(&message); DispatchMessageA(&message); }
    }
    if (self != INVALID_HANDLE_VALUE) CloseHandle(self);
    ExitProcess(install_ok ? 0 : (ui_page == 5 ? 1 : 0));
invalid:
    if (self != INVALID_HANDLE_VALUE) CloseHandle(self);
    MessageBoxA(NULL, "This setup file is incomplete or damaged. Download it again.",
                "RetroPlayer Setup", MB_OK|MB_ICONEXCLAMATION); ExitProcess(2);
}
#endif
