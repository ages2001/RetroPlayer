/* RetroPlayer Win32 front end. No CRT startup or post-386 instructions.
 * Loads the LibVLC 3.0.24 engine (ported to the i386 legacy target) that
 * ships next to RETROPLY.EXE as LIBVLC.DLL with its PLUGINS folder.
 *
 * The interface follows VLC: a video area over a control bar with a draggable
 * seek slider, elapsed/total time, play/pause/stop/seek buttons and a volume
 * slider. Everything is drawn with core GDI so it renders on NT 3.1 as well
 * as Windows 95 and later.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdarg.h>

#define RP_VERSION "1.0"

typedef struct libvlc_instance_t VLC;
typedef struct libvlc_media_t MEDIA;
typedef struct libvlc_media_player_t PLAYER;
typedef struct {
    VLC * (__cdecl *create)(int, const char *const *);
    void (__cdecl *release)(VLC *);
    MEDIA * (__cdecl *media)(VLC *, const char *);
    void (__cdecl *add_option)(MEDIA *, const char *);
    void (__cdecl *free_media)(MEDIA *);
    PLAYER * (__cdecl *player)(VLC *);
    void (__cdecl *free_player)(PLAYER *);
    void (__cdecl *set_media)(PLAYER *, MEDIA *);
    void (__cdecl *set_hwnd)(PLAYER *, void *);
    int (__cdecl *play)(PLAYER *);
    void (__cdecl *pause)(PLAYER *);
    void (__cdecl *stop)(PLAYER *);
    int (__cdecl *state)(PLAYER *);
    int (__cdecl *seekable)(PLAYER *);
    LONGLONG (__cdecl *time)(PLAYER *);
    LONGLONG (__cdecl *length)(PLAYER *);
    void (__cdecl *seek)(PLAYER *, LONGLONG);
    float (__cdecl *position)(PLAYER *);
    void (__cdecl *set_position)(PLAYER *, float);
    int (__cdecl *get_volume)(PLAYER *);
    int (__cdecl *set_volume)(PLAYER *, int);
    int (__cdecl *get_mute)(PLAYER *);
    void (__cdecl *set_mute)(PLAYER *, int);
    void (__cdecl *log_set)(VLC *,void (__cdecl *)(void *,int,const void *,const char *,va_list),void *);
} API;

static API api;
static HINSTANCE instance;
static HMODULE engine;
static VLC *vlc;
static PLAYER *player;
static HWND frame, video;
static HFONT ui_font;
static char directory[MAX_PATH], filename[MAX_PATH];
static WCHAR wide_filename[MAX_PATH];
static char utf8[MAX_PATH * 4];
static char current_file[MAX_PATH];
static BOOL smoke, has_media, have_initial, have_current;
static char initial_file[MAX_PATH];
static DWORD load_error;
static HANDLE log_file=INVALID_HANDLE_VALUE;
static int (__cdecl *format_log)(char *,unsigned,const char *,va_list);

/* UI state. */
static char status_text[256] = "  Ready - select File / Open to play a file.";
static int ui_state = 0;                 /* 0..7 libvlc state                */
static LONGLONG ui_time, ui_length;      /* current / total, in ms           */
static int ui_volume = 100;              /* 0..100                           */
static BOOL ui_muted;
static BOOL seek_drag, vol_drag;
static RECT seek_rect, vol_rect;         /* clickable track rectangles       */
static int vout_choice;                  /* 0 auto,1 gdi,2 ddraw,3 d3d9      */

static void __cdecl logger(void *opaque,int level,const void *context,const char *fmt,va_list args)
{
    char text[1024];
    DWORD written;
    (void)opaque; (void)level; (void)context;
    if (log_file==INVALID_HANDLE_VALUE) return;
    if (format_log) {
        format_log(text,sizeof(text)-1,fmt,args);
        text[sizeof(text)-1]=0;
        fmt=text;
    }
    WriteFile(log_file,fmt,(DWORD)lstrlenA(fmt),&written,NULL);
    WriteFile(log_file,"\r\n",2,&written,NULL);
}

enum { OPEN = 101, PLAY, PAUSE, STOP, QUIT, ABOUT, BACK, FORWARD, START,
       PLAYPAUSE, MUTE, VOUT_AUTO, VOUT_GDI, VOUT_DDRAW, VOUT_D3D9 };

/* Control-bar geometry. */
#define BARH   64         /* control bar height                 */
#define SEEKY  10         /* seek slider top offset within bar  */
#define SEEKH  10         /* seek slider track thickness        */
#define ROWY   30         /* control row top offset within bar  */
#define BTN    28         /* transport button size              */
#define GAP    4
#define MGN    10
#define VOLW   74         /* volume slider width                */

/* Light "XP media player" theme. Colours are chosen at startup from the screen
 * colour depth: a glossy blue-on-silver look with gradients on high-colour
 * displays, and flat system 3D colours (all in the 16-colour VGA palette) on
 * 16/256-colour displays so it still looks clean on NT 3.1 / 95 and Win 3.x. */
static BOOL hi_color;
static COLORREF c_bg;                 /* backdrop around the video        */
static COLORREF c_bar_top, c_bar_bot; /* control-bar gradient             */
static COLORREF c_sep;                /* separator / accent line          */
static COLORREF c_face, c_hi, c_sh;   /* button face / highlight / shadow */
static COLORREF c_glyph;              /* transport glyph                  */
static COLORREF c_acc1, c_acc2;       /* play button & slider fill (lt/dk)*/
static COLORREF c_track;              /* slider groove                    */
static COLORREF c_txt;                /* text                             */

static void fill_vgrad(HDC dc, int w, int top, int bottom, COLORREF c0, COLORREF c1);

static void init_theme(void)
{
    HDC sdc = GetDC(NULL);
    hi_color = (GetDeviceCaps(sdc, BITSPIXEL) * GetDeviceCaps(sdc, PLANES)) > 8;
    ReleaseDC(NULL, sdc);
    if (hi_color) {
        c_bg      = RGB(233,239,248);
        c_bar_top = RGB(253,254,255);
        c_bar_bot = RGB(203,215,236);
        c_sep     = RGB(120,150,200);
        c_face    = RGB(246,249,253);
        c_hi      = RGB(255,255,255);
        c_sh      = RGB(120,140,172);
        c_glyph   = RGB( 16, 74,160);
        c_acc1    = RGB( 96,170,250);
        c_acc2    = RGB( 24, 96,196);
        c_track   = RGB(198,208,224);
        c_txt     = RGB( 28, 44, 82);
    } else {
        c_bg      = GetSysColor(COLOR_3DFACE);
        c_bar_top = GetSysColor(COLOR_3DFACE);
        c_bar_bot = GetSysColor(COLOR_3DFACE);
        c_sep     = RGB(0,0,128);
        c_face    = GetSysColor(COLOR_3DFACE);
        c_hi      = GetSysColor(COLOR_3DHILIGHT);
        c_sh      = GetSysColor(COLOR_3DSHADOW);
        c_glyph   = RGB(0,0,128);
        c_acc1    = RGB(0,0,255);
        c_acc2    = RGB(0,0,128);
        c_track   = GetSysColor(COLOR_3DSHADOW);
        c_txt     = RGB(0,0,0);
    }
}

static const int bar_ids[] = { OPEN, 0, START, BACK, PLAYPAUSE, STOP, FORWARD };
#define BAR_COUNT ((int)(sizeof(bar_ids)/sizeof(bar_ids[0])))
static HWND bar_btn[BAR_COUNT];
static HWND mute_btn;
static int row_right;      /* x just past the last transport button */

static void zero(void *ptr, unsigned n)
{
    unsigned char *p = ptr;
    while (n--) *p++ = 0;
}

static BOOL playing_now(void) { return player && ui_state == 3; }

/* CheckMenuRadioItem is Win95/NT4+; CheckMenuItem exists on NT 3.1, so the
 * radio set is maintained by hand to keep the binary loadable on NT 3.1. */
static void check_vout_menu(HMENU m)
{
    int id;
    for (id = VOUT_AUTO; id <= VOUT_D3D9; ++id)
        CheckMenuItem(m, id, MF_BYCOMMAND | (id == VOUT_AUTO + vout_choice ? MF_CHECKED : MF_UNCHECKED));
}

static void fmt_time(char *out, LONGLONG ms)
{
    unsigned s, h, m, sec;
    if (ms < 0) ms = 0;
    s = (unsigned)ms / 1000;   /* 32-bit: durations fit; avoids 64-bit divide */
    h = s / 3600; m = (s % 3600) / 60; sec = s % 60;
    if (h) wsprintfA(out, "%u:%02u:%02u", h, m, sec);
    else   wsprintfA(out, "%u:%02u", m, sec);
}

static void set_status(const char *text)
{
    int i = 0;
    status_text[i++] = ' '; status_text[i++] = ' ';
    while (text[0] && i < (int)sizeof(status_text) - 1) status_text[i++] = *text++;
    status_text[i] = 0;
}

/* ---- drawing -------------------------------------------------------------- */

static void draw_glyph(HDC dc, RECT rc, int id, int off, COLORREF col)
{
    HBRUSH b = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    int cx = (rc.left + rc.right) / 2 + off, cy = (rc.top + rc.bottom) / 2 + off;
    POINT t[3];
    int glyph = id;
    if (glyph == PLAYPAUSE) glyph = playing_now() ? PAUSE : PLAY;
    switch (glyph) {
    case PLAY:
        t[0].x=cx-5; t[0].y=cy-6; t[1].x=cx-5; t[1].y=cy+6; t[2].x=cx+7; t[2].y=cy;
        Polygon(dc, t, 3);
        break;
    case PAUSE:
        Rectangle(dc, cx-6, cy-6, cx-2, cy+7);
        Rectangle(dc, cx+2, cy-6, cx+6, cy+7);
        break;
    case STOP:
        Rectangle(dc, cx-6, cy-6, cx+7, cy+7);
        break;
    case OPEN:
        t[0].x=cx-8; t[0].y=cy-2; t[1].x=cx-8; t[1].y=cy-5; t[2].x=cx-3; t[2].y=cy-5;
        Polygon(dc, t, 3);
        Rectangle(dc, cx-8, cy-3, cx+8, cy+6);
        break;
    case BACK:
        t[0].x=cx+1; t[0].y=cy-6; t[1].x=cx+1; t[1].y=cy+6; t[2].x=cx-5; t[2].y=cy; Polygon(dc,t,3);
        t[0].x=cx+7; t[0].y=cy-6; t[1].x=cx+7; t[1].y=cy+6; t[2].x=cx+1; t[2].y=cy; Polygon(dc,t,3);
        break;
    case FORWARD:
        t[0].x=cx-1; t[0].y=cy-6; t[1].x=cx-1; t[1].y=cy+6; t[2].x=cx+5; t[2].y=cy; Polygon(dc,t,3);
        t[0].x=cx-7; t[0].y=cy-6; t[1].x=cx-7; t[1].y=cy+6; t[2].x=cx-1; t[2].y=cy; Polygon(dc,t,3);
        break;
    case START:
        Rectangle(dc, cx-7, cy-6, cx-4, cy+7);
        t[0].x=cx+6; t[0].y=cy-6; t[1].x=cx+6; t[1].y=cy+6; t[2].x=cx-2; t[2].y=cy; Polygon(dc,t,3);
        break;
    case MUTE: {
        /* speaker: a back box and a cone drawn as one filled silhouette */
        POINT sp[6];
        HPEN wp; HGDIOBJ wo;
        sp[0].x=cx-8; sp[0].y=cy-3;
        sp[1].x=cx-4; sp[1].y=cy-3;
        sp[2].x=cx;   sp[2].y=cy-8;
        sp[3].x=cx;   sp[3].y=cy+8;
        sp[4].x=cx-4; sp[4].y=cy+3;
        sp[5].x=cx-8; sp[5].y=cy+3;
        Polygon(dc, sp, 6);
        if (ui_muted) {
            /* a red cross marks the muted state */
            wp = CreatePen(PS_SOLID, 2, RGB(210,0,0)); wo = SelectObject(dc, wp);
            MoveToEx(dc, cx+4,  cy-5, NULL); LineTo(dc, cx+10, cy+5);
            MoveToEx(dc, cx+10, cy-5, NULL); LineTo(dc, cx+4,  cy+5);
            SelectObject(dc, wo); DeleteObject(wp);
        } else {
            /* two chevron sound waves in the glyph colour */
            wp = CreatePen(PS_SOLID, 2, col); wo = SelectObject(dc, wp);
            MoveToEx(dc, cx+4, cy-4, NULL); LineTo(dc, cx+7,  cy); LineTo(dc, cx+4, cy+4);
            MoveToEx(dc, cx+8, cy-6, NULL); LineTo(dc, cx+12, cy); LineTo(dc, cx+8, cy+6);
            SelectObject(dc, wo); DeleteObject(wp);
        }
        break;
    }
    }
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(pen);
}

static void draw_button(LPDRAWITEMSTRUCT d)
{
    BOOL down = (d->itemState & ODS_SELECTED) != 0;
    RECT rc = d->rcItem;
    BOOL isplay = ((int)d->CtlID == PLAYPAUSE);
    COLORREF glyph = c_glyph;
    if (hi_color) {
        HRGN rgn = isplay
            ? CreateEllipticRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1)
            : CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, 7, 7);
        COLORREF t0, t1; HPEN pe; HGDIOBJ op, ob;
        if (isplay) { t0 = down ? c_acc2 : c_acc1; t1 = c_acc2; glyph = RGB(255,255,255); }
        else        { t0 = down ? RGB(205,218,238) : c_hi; t1 = down ? RGB(183,199,224) : RGB(214,224,241); }
        SelectClipRgn(d->hDC, rgn);
        fill_vgrad(d->hDC, rc.right, rc.top, rc.bottom, t0, t1);
        SelectClipRgn(d->hDC, NULL);
        pe = CreatePen(PS_SOLID, 1, isplay ? c_acc2 : c_sh);
        op = SelectObject(d->hDC, pe);
        ob = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
        if (isplay) Ellipse(d->hDC, rc.left, rc.top, rc.right, rc.bottom);
        else RoundRect(d->hDC, rc.left, rc.top, rc.right, rc.bottom, 7, 7);
        SelectObject(d->hDC, ob); SelectObject(d->hDC, op);
        DeleteObject(pe); DeleteObject(rgn);
    } else {
        HBRUSH fb = CreateSolidBrush(c_face);
        FillRect(d->hDC, &rc, fb); DeleteObject(fb);
        DrawEdge(d->hDC, &rc, down ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
    }
    draw_glyph(d->hDC, rc, (int)d->CtlID, down ? 1 : 0, glyph);
}

static void compute_tracks(int w, int h)
{
    seek_rect.left = MGN; seek_rect.right = w - MGN;
    seek_rect.top = h - BARH + SEEKY; seek_rect.bottom = seek_rect.top + SEEKH;
    vol_rect.right = w - MGN; vol_rect.left = vol_rect.right - VOLW;
    vol_rect.top = h - BARH + ROWY + (BTN - 6) / 2; vol_rect.bottom = vol_rect.top + 6;
}

static void layout_bar(int w, int h)
{
    int i, x = MGN, y = h - BARH + ROWY;
    for (i = 0; i < BAR_COUNT; ++i) {
        if (!bar_ids[i]) { x += GAP * 3; continue; }
        MoveWindow(bar_btn[i], x, y, BTN, BTN, TRUE);
        x += BTN + GAP;
    }
    row_right = x;
    /* mute button sits just left of the volume track */
    MoveWindow(mute_btn, vol_rect.left - BTN - GAP, y, BTN, BTN, TRUE);
    compute_tracks(w, h);
}

static void fill_vgrad(HDC dc, int w, int top, int bottom, COLORREF c0, COLORREF c1)
{
    int y, span = bottom - top;
    for (y = 0; y < span; ++y) {
        int r0 = GetRValue(c0) + (GetRValue(c1) - GetRValue(c0)) * y / span;
        int g0 = GetGValue(c0) + (GetGValue(c1) - GetGValue(c0)) * y / span;
        int b0 = GetBValue(c0) + (GetBValue(c1) - GetBValue(c0)) * y / span;
        HBRUSH bb = CreateSolidBrush(RGB(r0, g0, b0));
        RECT rl; rl.left = 0; rl.top = top + y; rl.right = w; rl.bottom = top + y + 1;
        FillRect(dc, &rl, bb);
        DeleteObject(bb);
    }
}

static void draw_track(HDC dc, const RECT *r, int fill_x)
{
    RECT t = *r, f = *r;
    HBRUSH b; HPEN pen; HGDIOBJ ob, op;
    int cy = (r->top + r->bottom) / 2;
    /* sunken groove */
    b = CreateSolidBrush(c_track); FillRect(dc, &t, b); DeleteObject(b);
    DrawEdge(dc, &t, EDGE_SUNKEN, BF_RECT);
    /* filled portion up to the thumb */
    if (fill_x > r->left) {
        f.right = fill_x;
        if (hi_color) {
            HRGN cl = CreateRectRgn(f.left, f.top, f.right, f.bottom);
            SelectClipRgn(dc, cl);
            fill_vgrad(dc, f.right, f.top, f.bottom, c_acc1, c_acc2);
            SelectClipRgn(dc, NULL); DeleteObject(cl);
        } else {
            b = CreateSolidBrush(c_acc1); FillRect(dc, &f, b); DeleteObject(b);
        }
    }
    /* thumb */
    if (fill_x < r->left) fill_x = r->left;
    if (fill_x > r->right) fill_x = r->right;
    if (hi_color) {
        int rad = 5;
        b = CreateSolidBrush(RGB(255,255,255));
        pen = CreatePen(PS_SOLID, 1, c_acc2);
        ob = SelectObject(dc, b); op = SelectObject(dc, pen);
        Ellipse(dc, fill_x - rad, cy - rad, fill_x + rad, cy + rad);
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(b); DeleteObject(pen);
    } else {
        RECT th;
        th.left = fill_x - 4; th.right = fill_x + 4;
        th.top = r->top - 3; th.bottom = r->bottom + 3;
        b = CreateSolidBrush(c_face); FillRect(dc, &th, b); DeleteObject(b);
        DrawEdge(dc, &th, EDGE_RAISED, BF_RECT);
    }
}

static void paint_frame(HWND window)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC dc = BeginPaint(window, &ps);
    int w, h, bar_top, fillx;
    HPEN pen; HGDIOBJ op, of;
    char left[32], right[32], line[80];
    GetClientRect(window, &rc);
    w = rc.right; h = rc.bottom;
    bar_top = h - BARH;
    /* backdrop around the video (video child paints its own black area) */
    {
        RECT bg; bg.left = 0; bg.top = 0; bg.right = w; bg.bottom = bar_top;
        HBRUSH bb = CreateSolidBrush(c_bg); FillRect(dc, &bg, bb); DeleteObject(bb);
    }
    /* control bar background */
    if (hi_color) fill_vgrad(dc, w, bar_top, h, c_bar_top, c_bar_bot);
    else {
        RECT br; br.left = 0; br.top = bar_top; br.right = w; br.bottom = h;
        HBRUSH bb = CreateSolidBrush(c_bar_top); FillRect(dc, &br, bb); DeleteObject(bb);
    }
    /* separator: a blue accent line, with a white sheen just below it on
     * high-colour displays for the glossy XP look */
    pen = CreatePen(PS_SOLID, 1, c_sep);
    op = SelectObject(dc, pen);
    MoveToEx(dc, 0, bar_top, NULL); LineTo(dc, w, bar_top);
    SelectObject(dc, op); DeleteObject(pen);
    if (hi_color) {
        pen = CreatePen(PS_SOLID, 1, RGB(255,255,255));
        op = SelectObject(dc, pen);
        MoveToEx(dc, 0, bar_top + 1, NULL); LineTo(dc, w, bar_top + 1);
        SelectObject(dc, op); DeleteObject(pen);
    }
    /* seek slider (32-bit integer math in seconds to avoid libgcc helpers) */
    {
        unsigned ts = (unsigned)ui_time / 1000, ls = (unsigned)ui_length / 1000;
        int trackw = seek_rect.right - seek_rect.left;
        fillx = seek_rect.left;
        if (ls > 0) {
            if (ts > ls) ts = ls;
            fillx += (int)((unsigned long)ts * (unsigned)trackw / ls);
        }
    }
    draw_track(dc, &seek_rect, fillx);
    /* volume slider */
    fillx = vol_rect.left + (ui_muted ? 0 : ui_volume) * (vol_rect.right - vol_rect.left) / 100;
    draw_track(dc, &vol_rect, fillx);
    /* time text and status, between the transport buttons and the volume */
    of = SelectObject(dc, ui_font ? (HGDIOBJ)ui_font : GetStockObject(ANSI_VAR_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c_txt);
    fmt_time(left, ui_time); fmt_time(right, ui_length);
    wsprintfA(line, "%s / %s", left, right);
    {
        int tx = (row_right > 0 ? row_right : MGN) + 12;
        int ty = bar_top + ROWY + (BTN - 14) / 2;
        TextOutA(dc, tx, ty, line, lstrlenA(line));
    }
    SelectObject(dc, of);
    EndPaint(window, &ps);
}

/* ---- UTF-8 ---------------------------------------------------------------- */

static BOOL wide_utf8(const WCHAR *wide)
{
    unsigned i, j = 0, c, lo, count = 0;
    while (count < MAX_PATH && wide[count]) ++count;
    if (count == MAX_PATH) return FALSE;
    for (i = 0; i < count; ++i) {
        c = wide[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            if (i + 1 >= count) return FALSE;
            lo = wide[++i];
            if (lo < 0xdc00 || lo > 0xdfff) return FALSE;
            c = 0x10000 + ((c - 0xd800) << 10) + lo - 0xdc00;
        } else if (c >= 0xdc00 && c <= 0xdfff) return FALSE;
        if (c < 0x80) utf8[j++] = (char)c;
        else if (c < 0x800) {
            utf8[j++] = (char)(0xc0 | (c >> 6));
            utf8[j++] = (char)(0x80 | (c & 63));
        } else if (c < 0x10000) {
            utf8[j++] = (char)(0xe0 | (c >> 12));
            utf8[j++] = (char)(0x80 | ((c >> 6) & 63));
            utf8[j++] = (char)(0x80 | (c & 63));
        } else {
            utf8[j++] = (char)(0xf0 | (c >> 18));
            utf8[j++] = (char)(0x80 | ((c >> 12) & 63));
            utf8[j++] = (char)(0x80 | ((c >> 6) & 63));
            utf8[j++] = (char)(0x80 | (c & 63));
        }
    }
    utf8[j] = 0;
    return TRUE;
}

static BOOL ansi_utf8(const char *src)
{
    WCHAR wide[MAX_PATH];
    if (!MultiByteToWideChar(CP_ACP, 0, src, -1, wide, MAX_PATH)) return FALSE;
    return wide_utf8(wide);
}

/* ---- engine --------------------------------------------------------------- */

static BOOL bind_engine(void)
{
    char path[MAX_PATH];
    if (lstrlenA(directory) + 24 >= MAX_PATH) return FALSE;
    lstrcpyA(path, directory);
    lstrcatA(path, "LIBVLC.DLL");
    engine = LoadLibraryA(path);
    if (!engine) { load_error = GetLastError(); return FALSE; }
#define BIND(member, symbol) do { \
    FARPROC proc = GetProcAddress(engine, symbol); \
    if (!proc) { load_error = GetLastError(); return FALSE; } \
    *(FARPROC *)&api.member = proc; \
} while (0)
    BIND(create, "libvlc_new");
    BIND(release, "libvlc_release");
    BIND(media, "libvlc_media_new_path");
    BIND(add_option, "libvlc_media_add_option");
    BIND(free_media, "libvlc_media_release");
    BIND(player, "libvlc_media_player_new");
    BIND(free_player, "libvlc_media_player_release");
    BIND(set_media, "libvlc_media_player_set_media");
    BIND(set_hwnd, "libvlc_media_player_set_hwnd");
    BIND(play, "libvlc_media_player_play");
    BIND(pause, "libvlc_media_player_pause");
    BIND(stop, "libvlc_media_player_stop");
    BIND(state, "libvlc_media_player_get_state");
    BIND(seekable, "libvlc_media_player_is_seekable");
    BIND(time, "libvlc_media_player_get_time");
    BIND(length, "libvlc_media_player_get_length");
    BIND(seek, "libvlc_media_player_set_time");
    BIND(position, "libvlc_media_player_get_position");
    BIND(set_position, "libvlc_media_player_set_position");
    BIND(get_volume, "libvlc_audio_get_volume");
    BIND(set_volume, "libvlc_audio_set_volume");
    BIND(get_mute, "libvlc_audio_get_mute");
    BIND(set_mute, "libvlc_audio_set_mute");
    BIND(log_set, "libvlc_log_set");
#undef BIND
    return TRUE;
}

static BOOL create_instance(void)
{
    char path[MAX_PATH];
    const char *args[] = { "--plugins-cache", "--plugins-scan", "--no-video-title-show" };
    lstrcpyA(path, directory);
    lstrcatA(path, "PLUGINS\\PLUGINS.DAT");
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        args[0] = "--reset-plugins-cache";
    vlc = api.create(sizeof(args)/sizeof(args[0]), args);
    if (!vlc) return FALSE;
    if (log_file == INVALID_HANDLE_VALUE) {
        lstrcpyA(path, directory); lstrcatA(path, "PLAYER.LOG");
        log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        format_log = (__typeof__(format_log))GetProcAddress(GetModuleHandleA("CRTDLL.DLL"), "_vsnprintf");
    }
    api.log_set(vlc, logger, NULL);
    player = api.player(vlc);
    if (!player) return FALSE;
    api.set_hwnd(player, video);
    return TRUE;
}

static BOOL load_engine(void)
{
    if (!bind_engine()) return FALSE;
    return create_instance();
}

static void apply_vout_env(void)
{
    /* Explicit choices keep a software GDI fallback so an unavailable
     * accelerator never leaves a black video area. */
    switch (vout_choice) {
    case 1: SetEnvironmentVariableA("RP_VOUT", "rpgdi"); break;
    case 2: SetEnvironmentVariableA("RP_VOUT", "directdraw,rpgdi"); break;
    case 3: SetEnvironmentVariableA("RP_VOUT", "direct3d9,rpgdi"); break;
    default: SetEnvironmentVariableA("RP_VOUT", NULL); break;   /* auto, per OS */
    }
}

/* ---- playback ------------------------------------------------------------- */

static void set_title(const char *ansi)
{
    char title[MAX_PATH + 16], *base = (char *)ansi;
    const char *p;
    int i = 0;
    for (p = ansi; *p; ++p) if (*p == '\\' || *p == '/' || *p == ':') base = (char *)p + 1;
    title[i++] = 'R'; title[i++] = 'e'; title[i++] = 't'; title[i++] = 'r';
    title[i++] = 'o'; title[i++] = 'P'; title[i++] = 'l'; title[i++] = 'a';
    title[i++] = 'y'; title[i++] = 'e'; title[i++] = 'r';
    if (*base) { title[i++] = ' '; title[i++] = '-'; title[i++] = ' '; }
    while (*base && i < (int)sizeof(title) - 1) title[i++] = *base++;
    title[i] = 0;
    SetWindowTextA(frame, title);
}

static void play_file(const char *ansi)
{
    MEDIA *m;
    if (!player) {
        MessageBoxA(frame, "The playback engine (LIBVLC.DLL and the PLUGINS\r\n"
                    "folder) was not found next to RETROPLY.EXE.", "RetroPlayer", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (!ansi_utf8(ansi)) { set_status("Cannot convert file name to UTF-8."); return; }
    m = api.media(vlc, utf8);
    if (!m) { set_status("LibVLC could not open the file."); return; }
    api.stop(player);
    api.set_media(player, m);
    api.free_media(m);
    has_media = TRUE;
    if (ansi != current_file) lstrcpynA(current_file, ansi, MAX_PATH);
    have_current = TRUE;
    ui_time = 0; ui_length = 0;
    set_title(ansi);
    if (api.play(player) < 0) set_status("LibVLC could not start playback.");
    else set_status("Opening...");
}

static void open_media(void)
{
    if (!player) {
        MessageBoxA(frame, "The playback engine (LIBVLC.DLL and the PLUGINS\r\n"
                    "folder) was not found next to RETROPLY.EXE.", "RetroPlayer", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (GetVersion() & 0x80000000UL) {
        OPENFILENAMEA of;
        zero(&of, sizeof(of));
        of.lStructSize = OPENFILENAME_SIZE_VERSION_400A;
        of.hwndOwner = frame;
        of.lpstrFilter = "Media files\0*.*\0\0";
        of.lpstrFile = filename;
        of.nMaxFile = MAX_PATH;
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameA(&of)) return;
        play_file(filename);
        return;
    }
    {
        OPENFILENAMEW of;
        zero(&of, sizeof(of));
        of.lStructSize = OPENFILENAME_SIZE_VERSION_400W;
        of.hwndOwner = frame;
        of.lpstrFilter = L"Media files\0*.*\0\0";
        of.lpstrFile = wide_filename;
        of.nMaxFile = MAX_PATH;
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&of)) return;
        if (!wide_utf8(wide_filename)) { set_status("Cannot convert file name to UTF-8."); return; }
        if (!WideCharToMultiByte(CP_ACP, 0, wide_filename, -1, current_file, MAX_PATH, NULL, NULL))
            current_file[0] = 0;
    }
    {
        MEDIA *m = api.media(vlc, utf8);
        if (!m) { set_status("LibVLC could not open the file."); return; }
        api.stop(player);
        api.set_media(player, m);
        api.free_media(m);
        has_media = TRUE;
        have_current = current_file[0] != 0;
        ui_time = 0; ui_length = 0;
        {
            char tt[MAX_PATH]; int n;
            n = WideCharToMultiByte(CP_ACP, 0, wide_filename, -1, tt, MAX_PATH, NULL, NULL);
            if (n > 0) set_title(tt); else SetWindowTextA(frame, "RetroPlayer");
        }
        if (api.play(player) < 0) set_status("LibVLC could not start playback.");
        else set_status("Opening...");
    }
}

/* Recreate the engine so a new RP_VOUT choice takes effect, then resume. */
static void reinit_engine(void)
{
    LONGLONG resume = ui_time;
    if (player) { api.stop(player); api.free_player(player); player = NULL; }
    if (vlc) { api.release(vlc); vlc = NULL; }
    apply_vout_env();
    if (!create_instance()) { set_status("Could not reinitialise the engine."); return; }
    if (api.set_volume) api.set_volume(player, ui_muted ? 0 : ui_volume);
    if (have_current && current_file[0]) {
        play_file(current_file);
        if (resume > 1000 && api.seekable && api.seekable(player)) api.seek(player, resume);
    }
}

/* ---- smoke test ----------------------------------------------------------- */

static BOOL report(void)
{
    char path[MAX_PATH], text[512];
    DWORD written, len, version = GetVersion();
    HANDLE file;
    lstrcpyA(path, directory);
    lstrcatA(path, "REPORT.TXT");
    wsprintfA(text, "RetroPlayer frontend smoke test\r\n"
              "OS=%u.%u family=%s\r\nGUI=PASS\r\n"
              "ENGINE=%s\r\nLOAD_ERROR=%lu\r\nPLAYBACK=UNTESTED\r\n",
              LOBYTE(LOWORD(version)), HIBYTE(LOWORD(version)),
              (version & 0x80000000UL) ? "9x" : "NT",
              player ? "LOADED" : "UNAVAILABLE", load_error);
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    len = (DWORD)lstrlenA(text);
    if (!WriteFile(file, text, len, &written, NULL) || written != len) {
        CloseHandle(file); return FALSE;
    }
    return CloseHandle(file);
}

/* ---- transport ------------------------------------------------------------ */

static void seek_relative(int command)
{
    LONGLONG target, duration;
    int state;
    if (!player || !has_media) return;
    state = api.state(player);
    if ((state != 3 && state != 4) || !api.seekable(player)) return;
    target = command == START ? 0 : api.time(player) + (command == BACK ? -10000 : 10000);
    duration = api.length(player);
    if (duration > 0 && target >= duration) target = duration - 1;
    if (target < 0) target = 0;
    api.seek(player, target);
    ui_time = target;
}

static void seek_fraction(int mouse_x)
{
    double frac;
    if (!player || !has_media || !api.seekable || !api.seekable(player)) return;
    frac = (double)(mouse_x - seek_rect.left) / (double)(seek_rect.right - seek_rect.left);
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    if (api.set_position) api.set_position(player, (float)frac);
    if (ui_length > 0) ui_time = (LONGLONG)(unsigned)(frac * (double)(unsigned)ui_length);
}

static void set_volume_x(int mouse_x)
{
    int v = (mouse_x - vol_rect.left) * 100 / (vol_rect.right - vol_rect.left);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    ui_volume = v; ui_muted = (v == 0);
    if (player && api.set_volume) api.set_volume(player, ui_volume);
    if (player && api.set_mute) api.set_mute(player, ui_muted ? 1 : 0);
}

static void toggle_playpause(void)
{
    if (!player || !has_media) { if (player && have_current) play_file(current_file); return; }
    if (api.state(player) >= 6) { api.stop(player); api.play(player); }
    else api.pause(player);   /* libvlc_media_player_pause toggles */
}

/* ---- window --------------------------------------------------------------- */

static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        compute_tracks(w, h);
        MoveWindow(video, 0, 0, w, h - BARH > 0 ? h - BARH : 0, TRUE);
        layout_bar(w, h);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_frame(window);
        return 0;
    case WM_DRAWITEM:
        draw_button((LPDRAWITEMSTRUCT)lp);
        return TRUE;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (x >= seek_rect.left-4 && x <= seek_rect.right+4 && y >= seek_rect.top-6 && y <= seek_rect.bottom+6) {
            seek_drag = TRUE; SetCapture(window); seek_fraction(x); InvalidateRect(window, NULL, FALSE);
        } else if (x >= vol_rect.left-4 && x <= vol_rect.right+4 && y >= vol_rect.top-8 && y <= vol_rect.bottom+8) {
            vol_drag = TRUE; SetCapture(window); set_volume_x(x); InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (seek_drag) { seek_fraction((short)LOWORD(lp)); InvalidateRect(window, NULL, FALSE); }
        else if (vol_drag) { set_volume_x((short)LOWORD(lp)); InvalidateRect(window, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP:
        if (seek_drag || vol_drag) { seek_drag = vol_drag = FALSE; ReleaseCapture(); }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case OPEN: open_media(); break;
        case PLAYPAUSE: toggle_playpause(); break;
        case PLAY:
            if (player && has_media) { if (api.state(player) >= 6) api.stop(player); api.play(player); }
            else if (player && have_current) play_file(current_file);
            break;
        case PAUSE: if (player) api.pause(player); break;
        case STOP: if (player) { api.stop(player); ui_time = 0; } break;
        case BACK: case FORWARD: case START: seek_relative(LOWORD(wp)); break;
        case MUTE:
            ui_muted = !ui_muted;
            if (player && api.set_mute) api.set_mute(player, ui_muted ? 1 : 0);
            if (!ui_muted && player && api.set_volume) api.set_volume(player, ui_volume);
            InvalidateRect(mute_btn, NULL, TRUE); InvalidateRect(window, NULL, FALSE);
            break;
        case VOUT_AUTO: case VOUT_GDI: case VOUT_DDRAW: case VOUT_D3D9: {
            int choice = LOWORD(wp) - VOUT_AUTO;
            if (choice != vout_choice) {
                HMENU m = GetMenu(window);
                vout_choice = choice;
                check_vout_menu(m);
                reinit_engine();
            }
            break;
        }
        case QUIT: DestroyWindow(window); break;
        case ABOUT: MessageBoxA(window,
                   "RetroPlayer version " RP_VERSION "\r\n"
                   "by ages2001\r\n"
                   "\r\n"
                   "Media player for Windows NT 3.1/3.5x/4, Windows 95/98/ME,\r\n"
                   "Windows 2000 and XP (32/64-bit).\r\n"
                   "\r\n"
                   "Playback engine: LibVLC 3.0.24 (VLC) with FFmpeg 8.1.2\r\n"
                   "and dav1d 1.5.4, ported to the i386 legacy target.\r\n"
                   "\r\n"
                   "Video output: software GDI on NT 3.x and modern Windows;\r\n"
                   "DirectDraw / Direct3D on 9x, NT 4, 2000 and XP (Video menu).\r\n"
                   "\r\n"
                   "Plays H.264/HEVC/VP9/AV1, MPEG-1/2/4, WMV/VC-1 and the\r\n"
                   "full VLC/FFmpeg decoder set in MP4, MKV, WebM, AVI, TS\r\n"
                   "and other containers.",
                   "About RetroPlayer", MB_OK | MB_ICONINFORMATION); break;
        }
        return 0;
    case WM_TIMER:
        if (smoke) {
            BOOL ok = report();
            DestroyWindow(window);
            if (!ok) PostQuitMessage(2);
        } else if (player && has_media) {
            int prev = ui_state;
            ui_state = api.state(player);
            if (!seek_drag) ui_time = api.time(player);
            ui_length = api.length(player);
            {
                const char *states[] = { "Stopped", "Opening", "Buffering", "Playing",
                                         "Paused", "Stopped", "Ended", "Playback error" };
                if (ui_state >= 0 && ui_state < 8) set_status(states[ui_state]);
            }
            if ((prev == 3) != (ui_state == 3)) InvalidateRect(bar_btn[4], NULL, TRUE);
            /* Repaint only the control bar; the video area belongs to the
             * engine's output module, so the front end never paints over it. */
            {
                RECT cr, br;
                GetClientRect(window, &cr);
                br.left = 0; br.top = cr.bottom - BARH; br.right = cr.right; br.bottom = cr.bottom;
                InvalidateRect(window, &br, FALSE);
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

static void parse_command_line(void)
{
    char *p = GetCommandLineA();
    if (*p == '"') { ++p; while (*p && *p != '"') ++p; if (*p == '"') ++p; }
    else while (*p && *p != ' ' && *p != '\t') ++p;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (*p == '/' || *p == '-') {
            char opt[16]; int n = 0; char *q = p + 1;
            while (*q && *q != ' ' && *q != '\t' && n < 15) opt[n++] = *q++;
            opt[n] = 0;
            if (lstrcmpiA(opt, "smoke") == 0) smoke = TRUE;
            p = q;
        } else {
            int n = 0;
            if (*p == '"') { ++p; while (*p && *p != '"' && n < MAX_PATH-1) initial_file[n++] = *p++; if (*p == '"') ++p; }
            else while (*p && *p != ' ' && *p != '\t' && n < MAX_PATH-1) initial_file[n++] = *p++;
            initial_file[n] = 0;
            have_initial = TRUE;
            break;
        }
    }
}

void WINAPI RPEntry(void)
{
    WNDCLASSA wc;
    MSG msg;
    HMENU menu, filemenu, playback, videomenu;
    char *p, *last;
    int result, i;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    instance = GetModuleHandleA(NULL);
    if (!GetModuleFileNameA(NULL, directory, MAX_PATH) || lstrlenA(directory) >= MAX_PATH-1)
        ExitProcess(1);
    last = directory;
    for (p = directory; *p; ++p) if (*p == '\\' || *p == '/') last = p+1;
    *last = 0;
    parse_command_line();
    init_theme();

    ui_font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          FF_SWISS | VARIABLE_PITCH, "MS Sans Serif");
    zero(&wc, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(1));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(c_bg);
    wc.lpszClassName = "RetroPlayer";
    if (!RegisterClassA(&wc)) ExitProcess(1);

    menu = CreateMenu(); filemenu = CreatePopupMenu();
    playback = CreatePopupMenu(); videomenu = CreatePopupMenu();
    AppendMenuA(filemenu, MF_STRING, OPEN, "&Open...");
    AppendMenuA(filemenu, MF_STRING, QUIT, "E&xit");
    AppendMenuA(playback, MF_STRING, PLAYPAUSE, "&Play / Pause");
    AppendMenuA(playback, MF_STRING, STOP, "&Stop");
    AppendMenuA(playback, MF_SEPARATOR, 0, NULL);
    AppendMenuA(playback, MF_STRING, BACK, "&Back 10 seconds");
    AppendMenuA(playback, MF_STRING, FORWARD, "&Forward 10 seconds");
    AppendMenuA(playback, MF_STRING, START, "Go to be&ginning");
    AppendMenuA(playback, MF_SEPARATOR, 0, NULL);
    AppendMenuA(playback, MF_STRING, MUTE, "&Mute");
    AppendMenuA(videomenu, MF_STRING, VOUT_AUTO,  "&Automatic (per OS)");
    AppendMenuA(videomenu, MF_STRING, VOUT_GDI,   "Software &GDI");
    AppendMenuA(videomenu, MF_STRING, VOUT_DDRAW, "&DirectDraw");
    AppendMenuA(videomenu, MF_STRING, VOUT_D3D9,  "Direct&3D 9");
    check_vout_menu(videomenu);
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)filemenu, "&File");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)playback, "&Playback");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)videomenu, "&Video");
    AppendMenuA(menu, MF_STRING, ABOUT, "&About");

    frame = CreateWindowExA(0, wc.lpszClassName, "RetroPlayer",
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 680, 500,
                           NULL, menu, instance, NULL);
    if (!frame) ExitProcess(1);
    video = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                           0, 0, 680, 420, frame, NULL, instance, NULL);
    if (!video) ExitProcess(1);
    for (i = 0; i < BAR_COUNT; ++i) {
        if (!bar_ids[i]) continue;
        bar_btn[i] = CreateWindowExA(0, "BUTTON", "", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        0, 0, BTN, BTN, frame, (HMENU)(UINT_PTR)bar_ids[i], instance, NULL);
        if (!bar_btn[i]) ExitProcess(1);
    }
    mute_btn = CreateWindowExA(0, "BUTTON", "", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        0, 0, BTN, BTN, frame, (HMENU)(UINT_PTR)MUTE, instance, NULL);
    if (!mute_btn) ExitProcess(1);

    if (load_engine()) {
        set_status("Ready - select File / Open to play a file.");
        if (api.set_volume) api.set_volume(player, ui_volume);
    } else {
        set_status("Playback engine (LIBVLC.DLL) not found next to RETROPLY.EXE.");
    }
    ShowWindow(frame, SW_SHOW);
    UpdateWindow(frame);
    if (have_initial && player && !smoke) play_file(initial_file);
    if (!SetTimer(frame, 1, smoke ? 1500 : 250, NULL)) ExitProcess(1);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (player) { api.stop(player); api.free_player(player); }
    if (vlc) api.release(vlc);
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    if (ui_font) DeleteObject(ui_font);
    if (engine) FreeLibrary(engine);
    ExitProcess(result < 0 ? 1 : (UINT)msg.wParam);
}
