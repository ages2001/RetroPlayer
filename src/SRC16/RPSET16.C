/* RPSET16.EXE - self-extracting 16-bit installer for RetroPlayer 16.
 *
 * A Next-style wizard: a Welcome/info page, an editable install-location page,
 * a confirmation page, then extraction. 8086 code, large model, Win16 KERNEL
 * file calls only, so it runs on Windows 3.0 and 3.1 (and later via WOW).
 *
 * The RETRO16.EXE payload is appended to this executable:
 *   [entry]...[entry][payload_size: u32 LE]["RPSX": 4 bytes]
 * each entry is
 *   [name_len: u16 LE][name][data_len: u32 LE][crc32: u32 LE][data]
 * The 16-bit payload is a flat set of DOS 8.3 files; it installs to the folder
 * chosen on the location page (default C:\RETROPLY). */
#include <windows.h>
#include <direct.h>      /* mkdir */
#include <ddeml.h>       /* Program Manager group creation (loaded at run time) */

#define DEFAULT_TARGET "C:\\RETROPLY"

/* Control ids kept away from IDOK(1)/IDCANCEL(2). */
#define RP_BACK   101
#define RP_NEXT   102
#define RP_CANCEL 103

static char buffer[8192];
static char g_target[160] = DEFAULT_TARGET;
static char g_self[160];
static HFILE g_in = HFILE_ERROR;
static LONG g_start, g_end;             /* payload bounds in the EXE file */
static HINSTANCE g_inst;
static HWND g_wnd, ui_title, ui_body, ui_edit, ui_back, ui_next, ui_cancel;
static HFONT ui_font, ui_font_bold;
static int ui_page;                     /* 0 welcome,1 path,2 confirm,3 install,4 done,5 error */
static int ui_cw, ui_ch;
static int install_ok;

static WORD rd16(const BYTE *p) { return (WORD)(p[0] | (p[1] << 8)); }
static DWORD rd32(const BYTE *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static void set_btn(HWND b, LPCSTR txt, BOOL show, BOOL enable)
{
    SetWindowText(b, txt);
    ShowWindow(b, show ? SW_SHOW : SW_HIDE);
    EnableWindow(b, enable);
}

static void show_page(int p)
{
    char body[600];
    ui_page = p;
    ShowWindow(ui_edit, (p == 1) ? SW_SHOW : SW_HIDE);
    switch (p) {
    case 0:
        SetWindowText(ui_title, "Welcome to RetroPlayer 16 Setup");
        SetWindowText(ui_body,
            "RetroPlayer version 1.0\r\nby ages2001\r\n\r\n"
            "This installs the 16-bit RetroPlayer media player on your computer. "
            "It runs on Windows 3.0 and 3.1.\r\n\r\n"
            "Close any running copy of RetroPlayer, then click Next to continue.");
        set_btn(ui_back, "< Back", TRUE, FALSE);
        set_btn(ui_next, "Next >", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        break;
    case 1:
        SetWindowText(ui_title, "Choose Install Location");
        SetWindowText(ui_body,
            "Setup will install RetroPlayer into the folder shown below.\r\n\r\n"
            "To use a different folder, edit the path. Then click Next to continue.");
        SetWindowText(ui_edit, g_target);
        set_btn(ui_back, "< Back", TRUE, TRUE);
        set_btn(ui_next, "Next >", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        SetFocus(ui_edit);
        break;
    case 2:
        SetWindowText(ui_title, "Ready to Install");
        wsprintf(body,
            "RetroPlayer is ready to be installed into:\r\n\r\n    %s\r\n\r\n"
            "Make sure RetroPlayer is not running, then click Install to begin.", (LPSTR)g_target);
        SetWindowText(ui_body, body);
        set_btn(ui_back, "< Back", TRUE, TRUE);
        set_btn(ui_next, "Install", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", TRUE, TRUE);
        break;
    case 3:
        SetWindowText(ui_title, "Installing");
        SetWindowText(ui_body, "Installing files, please wait...");
        set_btn(ui_back, "< Back", TRUE, FALSE);
        set_btn(ui_next, "Install", TRUE, FALSE);
        set_btn(ui_cancel, "Cancel", TRUE, FALSE);
        break;
    case 4:
        SetWindowText(ui_title, "Installation Complete");
        wsprintf(body,
            "RetroPlayer was installed into:\r\n\r\n    %s\r\n\r\n"
            "Run RETRO16.EXE from that folder to start the player.", (LPSTR)g_target);
        SetWindowText(ui_body, body);
        set_btn(ui_back, "< Back", FALSE, FALSE);
        set_btn(ui_next, "Finish", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", FALSE, FALSE);
        break;
    default: /* 5 error */
        SetWindowText(ui_title, "Installation Failed");
        SetWindowText(ui_body,
            "Installation could not be completed.\r\n"
            "The setup file may be incomplete or the disk may be full.");
        set_btn(ui_back, "< Back", FALSE, FALSE);
        set_btn(ui_next, "Close", TRUE, TRUE);
        set_btn(ui_cancel, "Cancel", FALSE, FALSE);
        break;
    }
}

/* Read the edit field into g_target, trimming and sanity-checking the path. */
static BOOL capture_path(void)
{
    char a[160]; int n, s = 0, i;
    n = GetWindowText(ui_edit, a, sizeof(a));
    while (n > 0 && (a[n-1] == ' ' || a[n-1] == '\t')) a[--n] = 0;
    while (n > 3 && a[n-1] == '\\') a[--n] = 0;           /* keep "C:\" intact */
    while (a[s] == ' ' || a[s] == '\t') ++s;
    if (s) { i = 0; do { a[i] = a[s+i]; } while (a[i++]); n -= s; }
    if (n < 3 || a[1] != ':' || a[2] != '\\') {
        MessageBox(g_wnd, "Enter a full path such as C:\\RETROPLY.",
                   "RetroPlayer Setup", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    lstrcpy(g_target, a);
    return TRUE;
}

/* Extract the flat payload into g_target. Returns FALSE on any I/O failure. */
static BOOL extract(void)
{
    char path[200], name[80];
    BYTE hdr[8];
    LONG pos;
    HFILE out;
    DWORD dlen, left;
    WORD nlen, chunk;

    mkdir(g_target);                                      /* ignored if it exists */
    pos = g_start;
    while (pos < g_end) {
        _llseek(g_in, pos, 0);
        if (_lread(g_in, (LPSTR)hdr, 2) != 2) return FALSE;
        pos += 2L;
        nlen = rd16(hdr);
        if (nlen == 0 || nlen >= sizeof(name)) return FALSE;
        _llseek(g_in, pos, 0);
        if (_lread(g_in, (LPSTR)name, nlen) != nlen) return FALSE;
        pos += (LONG)nlen;
        name[nlen] = 0;
        _llseek(g_in, pos, 0);
        if (_lread(g_in, (LPSTR)hdr, 8) != 8) return FALSE;  /* data_len + crc32 (crc unused) */
        pos += 8L;
        dlen = rd32(hdr);

        lstrcpy(path, g_target);
        lstrcat(path, "\\");
        lstrcat(path, name);
        out = _lcreat(path, 0);
        if (out == HFILE_ERROR) return FALSE;
        left = dlen;
        while (left) {
            chunk = (left < sizeof(buffer)) ? (WORD)left : (WORD)sizeof(buffer);
            _llseek(g_in, pos, 0);
            if (_lread(g_in, (LPSTR)buffer, chunk) != chunk) { _lclose(out); return FALSE; }
            if (_lwrite(out, (LPSTR)buffer, chunk) != chunk) { _lclose(out); return FALSE; }
            pos += (LONG)chunk;
            left -= chunk;
        }
        _lclose(out);
    }
    return TRUE;
}

/* DDEML is loaded at run time: Windows 3.0 may not ship DDEML.DLL, and a
 * missing static import would stop the installer from starting there. */
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

/* Add a Program Manager group with a RetroPlayer item and a red-X uninstaller
 * item. Best-effort: if DDEML or Program Manager is unavailable, it is skipped
 * and the installation still succeeds. */
static void create_pm_icons(void)
{
    HINSTANCE dll;
    P_DdeInitialize pInit; P_DdeCreateStringHandle pStr; P_DdeConnect pConn;
    P_DdeClientTransaction pTx; P_DdeDisconnect pDisc; P_DdeUninitialize pUninit;
    P_DdeFreeStringHandle pFree;
    DWORD inst = 0; HSZ s, t; HCONV conv; char cmd[512];

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
        lstrcpy(cmd, "[CreateGroup(RetroPlayer)]");
        pTx((LPVOID)cmd, lstrlen(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
        lstrcpy(cmd, "[ShowGroup(RetroPlayer,1)]");
        pTx((LPVOID)cmd, lstrlen(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
        wsprintf(cmd, "[AddItem(%s\\RETRO16.EXE,RetroPlayer,%s\\RETRO16.EXE,0)]",
                 (LPSTR)g_target, (LPSTR)g_target);
        pTx((LPVOID)cmd, lstrlen(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
        wsprintf(cmd, "[AddItem(%s\\RPUN16.EXE,Uninstall RetroPlayer,%s\\RPUN16.EXE,0)]",
                 (LPSTR)g_target, (LPSTR)g_target);
        pTx((LPVOID)cmd, lstrlen(cmd) + 1, conv, 0, 0, XTYP_EXECUTE, 3000, NULL);
        pDisc(conv);
    }
    pUninit(inst);
    FreeLibrary(dll);
}

static void do_install(void)
{
    show_page(3);
    UpdateWindow(g_wnd);
    if (extract()) { install_ok = 1; create_pm_icons(); show_page(4); }
    else           { show_page(5); }
    if (g_in != HFILE_ERROR) { _lclose(g_in); g_in = HFILE_ERROR; }
}

LRESULT FAR PASCAL _export SetupProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE: {
        RECT rc; int by, bw = 88, bh = 26, gap = 8, cx, nx, bx;
        GetClientRect(h, &rc); ui_cw = rc.right; ui_ch = rc.bottom;
        ui_title = CreateWindow("STATIC", "", WS_CHILD|WS_VISIBLE|SS_LEFT,
            20, 16, ui_cw-40, 24, h, (HMENU)0, g_inst, NULL);
        ui_body = CreateWindow("STATIC", "", WS_CHILD|WS_VISIBLE|SS_LEFT,
            20, 48, ui_cw-40, 190, h, (HMENU)0, g_inst, NULL);
        ui_edit = CreateWindow("EDIT", "", WS_CHILD|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,
            20, 132, ui_cw-40, 24, h, (HMENU)0, g_inst, NULL);
        by = ui_ch - 40;
        cx = ui_cw - 14 - bw; nx = cx - gap - bw; bx = nx - gap - bw;
        ui_back   = CreateWindow("BUTTON", "< Back", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,    bx, by, bw, bh, h, (HMENU)RP_BACK,   g_inst, NULL);
        ui_next   = CreateWindow("BUTTON", "Next >", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON, nx, by, bw, bh, h, (HMENU)RP_NEXT,   g_inst, NULL);
        ui_cancel = CreateWindow("BUTTON", "Cancel", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,    cx, by, bw, bh, h, (HMENU)RP_CANCEL, g_inst, NULL);
        SendMessage(ui_title,  WM_SETFONT, (WPARAM)ui_font_bold, 0L);
        SendMessage(ui_body,   WM_SETFONT, (WPARAM)ui_font, 0L);
        SendMessage(ui_edit,   WM_SETFONT, (WPARAM)ui_font, 0L);
        SendMessage(ui_back,   WM_SETFONT, (WPARAM)ui_font, 0L);
        SendMessage(ui_next,   WM_SETFONT, (WPARAM)ui_font, 0L);
        SendMessage(ui_cancel, WM_SETFONT, (WPARAM)ui_font, 0L);
        show_page(0);
        SetFocus(ui_next);
        return 0;
    }
    case WM_COMMAND:
        switch (wp) {                                     /* Win16: wParam is the id */
        case RP_BACK:
            if (ui_page == 1) show_page(0);
            else if (ui_page == 2) show_page(1);
            return 0;
        case RP_NEXT:
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
        if (ui_page != 3) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

static BOOL locate_payload(void)
{
    BYTE hdr[8];
    LONG filesize;
    DWORD payload_size;

    if (!GetModuleFileName(g_inst, g_self, sizeof(g_self))) return FALSE;
    g_in = _lopen(g_self, OF_READ);
    if (g_in == HFILE_ERROR) return FALSE;
    filesize = _llseek(g_in, 0L, 2);
    if (filesize < 8L) return FALSE;
    _llseek(g_in, filesize - 8L, 0);
    if (_lread(g_in, (LPSTR)hdr, 8) != 8) return FALSE;
    if (hdr[4] != 'R' || hdr[5] != 'P' || hdr[6] != 'S' || hdr[7] != 'X') return FALSE;
    payload_size = rd32(hdr);
    if (payload_size == 0 || (LONG)payload_size > filesize - 8L) return FALSE;
    g_start = filesize - 8L - (LONG)payload_size;
    g_end = filesize - 8L;
    return TRUE;
}

int PASCAL WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    MSG msg;

    (void)cmd; (void)show;
    g_inst = hinst;

    if (!locate_payload()) {
        if (g_in != HFILE_ERROR) _lclose(g_in);
        MessageBox(0, "This setup file is incomplete or damaged.",
                   "RetroPlayer Setup", MB_OK | MB_ICONSTOP);
        return 2;
    }

    ui_font = (HFONT)GetStockObject(ANSI_VAR_FONT);
    ui_font_bold = CreateFont(19, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_SWISS | VARIABLE_PITCH, "MS Sans Serif");
    if (!ui_font_bold) ui_font_bold = ui_font;

    if (!prev) {
        wc.style = 0;
        wc.lpfnWndProc = SetupProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hinst;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszMenuName = NULL;
        wc.lpszClassName = "RPSET16";
        if (!RegisterClass(&wc)) { _lclose(g_in); return 1; }
    }

    g_wnd = CreateWindow("RPSET16", "RetroPlayer Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
        NULL, NULL, hinst, NULL);
    if (!g_wnd) { _lclose(g_in); return 1; }
    ShowWindow(g_wnd, SW_SHOW);
    UpdateWindow(g_wnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            if (IsWindowEnabled(ui_next)) SendMessage(g_wnd, WM_COMMAND, RP_NEXT, 0L);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            if (IsWindowEnabled(ui_cancel)) SendMessage(g_wnd, WM_COMMAND, RP_CANCEL, 0L);
            continue;
        }
        if (!IsDialogMessage(g_wnd, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    if (g_in != HFILE_ERROR) _lclose(g_in);
    return install_ok ? 0 : (ui_page == 5 ? 1 : (int)msg.wParam);
}
