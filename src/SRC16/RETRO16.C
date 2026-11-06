/* RetroPlayer 16 - native Windows 3.0/3.1 media player.
 *
 * Runs in Windows 3.0 real, standard and 386 enhanced mode and Windows 3.1
 * standard and 386 enhanced mode. Built as a 16-bit NE executable.
 *
 * Playback paths:
 *  - Built-in software decoding (DECODE.C) for WAVE, VOC, AU and AIFF audio,
 *    output through MMSYSTEM waveOut or, without a wave driver, directly to a
 *    Sound Blaster compatible DSP (AOUT.C).
 *  - Everything else (MIDI, Video for Windows AVI, CD audio, ...) through the
 *    Media Control Interface when MMSYSTEM.DLL and a matching MCI driver are
 *    installed. Without MMSYSTEM these files are reported as unsupported.
 *
 * The 32-bit LibVLC player (RETRO.EXE) cannot run on Windows 3.x or Win32s:
 * Win32s provides no threads (see RESULTS/W32STEST.TXT). */
#include <windows.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <string.h>
#include "DECODE.H"
#include "AOUT.H"
#include "RETRO16.H"

#define UI_TIMER_ID 0x55
#define DECODE_FRAMES 1024

static HINSTANCE instance;
static HWND frame, status_window;
static char filename[260];
static char status_text[200];
static char info_text[200];

/* Built-in decoder state lives in one movable global block (real mode safe:
 * it is locked only while a message is being processed). */
typedef struct tagPLAYBACK {
    RPSTREAM stream;
    short decoded[DECODE_FRAMES * RP_MAX_CHANNELS];
    WORD decoded_frames, decoded_used;
    short last[RP_MAX_CHANNELS];   /* previous input frame (interpolation) */
    DWORD step;                    /* input frames per output frame, 16.16 */
    DWORD phase;                   /* fractional input position, 16.16 */
    BOOL primed;
    BOOL eof;
    AOFORMAT out;
    DWORD seek_base;               /* source frame where output restarted */
} PLAYBACK;

static HGLOBAL playback_block;
static BOOL have_stream;
static BOOL use_mci;
static BOOL mci_open_ok;

typedef DWORD (WINAPI *MCI_STRING)(LPCSTR, LPSTR, UINT, HWND);
typedef BOOL (WINAPI *MCI_ERROR)(DWORD, LPSTR, UINT);
static MCI_STRING mci_string;
static MCI_ERROR mci_error;

/* --------------------------------------------------------------- helpers */

/* a * n / d without floating point or 32-bit overflow. */
static DWORD muldiv(DWORD a, DWORD n, DWORD d)
{
    unsigned long long v = (unsigned long long)a * n;
    return d ? (DWORD)(v / d) : 0;
}

static long parse_long(LPCSTR p)
{
    long v = 0;
    BOOL negative = FALSE;
    while (*p == ' ') ++p;
    if (*p == '-') { negative = TRUE; ++p; }
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return negative ? -v : v;
}

static void set_status(LPCSTR text)
{
    lstrcpy(status_text, text);
    if (status_window) InvalidateRect(status_window, NULL, TRUE);
}

static LPCSTR mode_name(void)
{
    DWORD flags = GetWinFlags();
    if (flags & WF_ENHANCED) return "386 enhanced";
    if (flags & WF_STANDARD) return "standard";
    return "real";
}

static BOOL mci_available(void)
{
    HINSTANCE mm;
    if (mci_string) return TRUE;
    if (!AoHasMmsystem()) return FALSE;
    mm = GetModuleHandle("MMSYSTEM");
    if (!mm) return FALSE;
    mci_string = (MCI_STRING)GetProcAddress(mm, "mciSendString");
    mci_error = (MCI_ERROR)GetProcAddress(mm, "mciGetErrorString");
    return mci_string != NULL;
}

static DWORD mci(LPCSTR command, LPSTR reply, UINT size)
{
    if (reply && size) reply[0] = 0;
    return mci_string(command, reply, size, frame);
}

static void mci_close(void)
{
    if (mci_open_ok) mci("close rpmedia", NULL, 0);
    mci_open_ok = FALSE; use_mci = FALSE;
}

/* ----------------------------------------------------- built-in playback */

static void lock_playback(PLAYBACK FAR * FAR *p) { *p = playback_block ? (PLAYBACK FAR *)GlobalLock(playback_block) : NULL; }
static void unlock_playback(void) { if (playback_block) GlobalUnlock(playback_block); }

static BOOL refill_input(PLAYBACK FAR *p)
{
    int n;
    if (p->eof) return FALSE;
    n = RpDecode(&p->stream, p->decoded, DECODE_FRAMES);
    if (n <= 0) { p->eof = TRUE; p->decoded_frames = p->decoded_used = 0; return FALSE; }
    p->decoded_frames = (WORD)n; p->decoded_used = 0;
    return TRUE;
}

static void put_frame(PLAYBACK FAR *p, BYTE FAR *dst, WORD index, long l, long r)
{
    if (p->out.channels == 1) { l = (l + r) / 2; r = l; }
    if (p->out.bits == 8) {
        dst[index * p->out.channels] = (BYTE)((l >> 8) + 128);
        if (p->out.channels == 2) dst[index * 2 + 1] = (BYTE)((r >> 8) + 128);
    } else {
        short FAR *d = (short FAR *)dst;
        d[index * p->out.channels] = (short)l;
        if (p->out.channels == 2) d[index * 2 + 1] = (short)r;
    }
}

/* Linear-interpolating resampler with channel and sample-size conversion. */
static WORD FAR fill_output(void FAR *context, BYTE FAR *dst, WORD max_frames)
{
    PLAYBACK FAR *p;
    WORD produced = 0, nch;
    (void)context;
    lock_playback(&p);
    if (!p) return 0;
    nch = p->stream.channels;
    if (!p->primed) {
        if (!refill_input(p)) { unlock_playback(); return 0; }
        p->last[0] = p->decoded[0];
        p->last[1] = p->decoded[nch > 1 ? 1 : 0];
        p->decoded_used = 1;
        p->phase = 0;
        p->primed = TRUE;
    }
    while (produced < max_frames) {
        short l1, r1;
        long frac, l, r;
        /* Advance whole input frames. */
        while (p->phase >= 0x10000UL) {
            if (p->decoded_used >= p->decoded_frames && !refill_input(p)) goto done;
            p->last[0] = p->decoded[p->decoded_used * nch];
            p->last[1] = p->decoded[p->decoded_used * nch + (nch > 1 ? 1 : 0)];
            ++p->decoded_used;
            p->phase -= 0x10000UL;
        }
        if (p->decoded_used >= p->decoded_frames && !refill_input(p)) {
            /* Last frame: output it without interpolation partner. */
            if (p->phase == 0) { put_frame(p, dst, produced++, p->last[0], p->last[1]); p->phase = 0x10000UL; }
            goto done;
        }
        l1 = p->decoded[p->decoded_used * nch];
        r1 = p->decoded[p->decoded_used * nch + (nch > 1 ? 1 : 0)];
        frac = (long)(p->phase >> 1) & 0x7FFF;              /* 15-bit fraction */
        l = p->last[0] + (((long)(l1 - p->last[0]) * frac) >> 15);
        r = p->last[1] + (((long)(r1 - p->last[1]) * frac) >> 15);
        put_frame(p, dst, produced++, l, r);
        p->phase += p->step;
    }
done:
    unlock_playback();
    return produced;
}

static void close_media(void)
{
    AoStop();
    if (frame) KillTimer(frame, AO_TIMER_ID);
    if (playback_block) {
        PLAYBACK FAR *p;
        lock_playback(&p);
        if (p && have_stream) RpClose(&p->stream);
        unlock_playback();
        GlobalFree(playback_block);
        playback_block = 0;
    }
    have_stream = FALSE;
    mci_close();
    info_text[0] = 0;
}

static BOOL start_builtin(void)
{
    PLAYBACK FAR *p;
    AOFORMAT wanted;
    char error[96];
    BOOL ok;
    lock_playback(&p);
    if (!p) return FALSE;
    wanted.rate = p->stream.rate;
    wanted.channels = p->stream.channels;
    wanted.bits = 16;
    ok = AoOpen(frame, &wanted, &p->out, fill_output, NULL, error);
    if (ok) {
        p->step = (DWORD)(((p->stream.rate << 8) / p->out.rate) << 8);
        if (!p->step) p->step = 1;
        p->primed = FALSE; p->eof = FALSE;
        p->seek_base = p->stream.frame_pos;
        wsprintf(info_text, "%s, %lu Hz, %u ch -> %lu Hz %u-bit %u ch",
                 RpCodecName(&p->stream), p->stream.rate, p->stream.channels,
                 p->out.rate, p->out.bits, p->out.channels);
    }
    unlock_playback();
    if (!ok) { set_status(error); return FALSE; }
    if (!AoStart()) { set_status("Audio output could not start."); return FALSE; }
    return TRUE;
}

static void restart_at(DWORD source_frame)
{
    PLAYBACK FAR *p;
    AOSTATUS st;
    AoGetStatus(&st);
    if (st.backend == AO_NONE) return;
    AoReset();
    lock_playback(&p);
    if (!p) return;
    RpSeek(&p->stream, source_frame);
    p->seek_base = p->stream.frame_pos;
    p->primed = FALSE; p->eof = FALSE;
    unlock_playback();
    AoStart();
}

static DWORD current_source_frame(void)
{
    PLAYBACK FAR *p;
    AOSTATUS st;
    DWORD position = 0;
    AoGetStatus(&st);
    lock_playback(&p);
    if (p && p->out.rate) position = p->seek_base + muldiv(st.frames_played, p->stream.rate, p->out.rate);
    unlock_playback();
    return position;
}

/* ------------------------------------------------------------- commands */

static BOOL builtin_open(LPCSTR path)
{
    PLAYBACK FAR *p;
    BOOL ok;
    playback_block = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(PLAYBACK));
    if (!playback_block) { set_status("Out of memory."); return FALSE; }
    lock_playback(&p);
    ok = RpOpen(&p->stream, path);
    if (!ok) lstrcpy(status_text, p->stream.error);
    unlock_playback();
    if (!ok) { GlobalFree(playback_block); playback_block = 0; return FALSE; }
    have_stream = TRUE;
    return TRUE;
}

static BOOL mci_open(LPCSTR path)
{
    char command[340], reply[128];
    DWORD error;
    if (!mci_available()) return FALSE;
    wsprintf(command, "open %s alias rpmedia", (LPSTR)path);
    error = mci(command, reply, sizeof(reply));
    if (error) {
        if (mci_error) mci_error(error, reply, sizeof(reply)); else lstrcpy(reply, "MCI could not open the file.");
        set_status(reply);
        return FALSE;
    }
    mci_open_ok = TRUE; use_mci = TRUE;
    mci("set rpmedia time format milliseconds", NULL, 0);
    mci("capability rpmedia device type", reply, sizeof(reply));
    wsprintf(info_text, "MCI device: %s", (LPSTR)reply);
    /* Video devices: show the picture inside our window when possible. */
    wsprintf(command, "window rpmedia handle %u", (UINT)frame);
    mci(command, NULL, 0);
    return TRUE;
}

static void play_media(void)
{
    if (use_mci) { mci("play rpmedia notify", NULL, 0); set_status("Playing (MCI)."); return; }
    if (!have_stream) { set_status("Open a file first."); return; }
    {
        AOSTATUS st;
        AoGetStatus(&st);
        if (st.backend != AO_NONE && st.paused) { AoPause(FALSE); set_status("Playing."); return; }
        if (st.backend != AO_NONE && st.playing) return;
        if (st.backend != AO_NONE) { restart_at(0); set_status("Playing."); return; }
    }
    if (start_builtin()) set_status("Playing.");
}

static void open_media(LPCSTR path)
{
    close_media();
    lstrcpy(filename, path);
    if (builtin_open(path)) { play_media(); return; }
    if (mci_open(path)) { play_media(); return; }
    if (!mci_available()) {
        char text[240];
        wsprintf(text, "%s (MMSYSTEM is not installed, so MCI formats are unavailable.)", (LPSTR)status_text);
        set_status(text);
    }
}

static void pause_media(void)
{
    if (use_mci) {
        char mode[32];
        mci("status rpmedia mode", mode, sizeof(mode));
        if (!lstrcmpi(mode, "paused")) mci("resume rpmedia", NULL, 0); else mci("pause rpmedia", NULL, 0);
        return;
    }
    {
        AOSTATUS st;
        AoGetStatus(&st);
        if (st.backend == AO_NONE) return;
        AoPause(!st.paused);
        set_status(st.paused ? "Playing." : "Paused.");
    }
}

static void stop_media(void)
{
    if (use_mci) { mci("stop rpmedia", NULL, 0); mci("seek rpmedia to start", NULL, 0); set_status("Stopped."); return; }
    if (!have_stream) return;
    AoStop();
    {
        PLAYBACK FAR *p;
        lock_playback(&p);
        if (p) { RpSeek(&p->stream, 0); p->seek_base = 0; }
        unlock_playback();
    }
    set_status("Stopped.");
}

static void seek_media(long delta_ms, BOOL absolute)
{
    if (use_mci) {
        char reply[32], command[64];
        long position = 0, length = 0;
        mci("status rpmedia position", reply, sizeof(reply)); position = parse_long(reply);
        mci("status rpmedia length", reply, sizeof(reply)); length = parse_long(reply);
        position = absolute ? 0 : position + delta_ms;
        if (position < 0) position = 0;
        if (length > 0 && position >= length) position = length - 1;
        wsprintf(command, "play rpmedia from %ld notify", position);
        mci(command, NULL, 0);
        return;
    }
    if (!have_stream) return;
    {
        PLAYBACK FAR *p;
        DWORD rate, total, now;
        long target;
        lock_playback(&p);
        rate = p ? p->stream.rate : 0; total = p ? p->stream.total_frames : 0;
        unlock_playback();
        if (!rate) return;
        now = current_source_frame();
        target = absolute ? 0 : (long)now + delta_ms * (long)(rate / 1000);
        if (target < 0) target = 0;
        if (total && (DWORD)target >= total) target = (long)total - 1;
        restart_at((DWORD)target);
    }
}

/* ---------------------------------------------------------- file dialog */

typedef BOOL (WINAPI *GET_OPEN)(OPENFILENAME FAR *);

static BOOL commdlg_open(HWND owner, LPSTR path, UINT size)
{
    HINSTANCE dll;
    GET_OPEN get_open;
    OPENFILENAME of;
    BOOL ok;
    OFSTRUCT test;
    char system_path[160];
    int length = GetSystemDirectory(system_path, sizeof(system_path) - 16);
    if (length <= 0) return FALSE;
    if (system_path[length - 1] != '\\') system_path[length++] = '\\';
    lstrcpy(system_path + length, "COMMDLG.DLL");
    if (OpenFile(system_path, &test, OF_EXIST) == HFILE_ERROR) return FALSE;   /* Windows 3.0 */
    dll = LoadLibrary("COMMDLG.DLL");
    if ((UINT)dll < 32) return FALSE;
    get_open = (GET_OPEN)GetProcAddress(dll, "GetOpenFileName");
    if (!get_open) { FreeLibrary(dll); return FALSE; }
    _fmemset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of);
    of.hwndOwner = owner;
    of.lpstrFilter = "Media files\0*.WAV;*.VOC;*.AU;*.SND;*.AIF;*.AIFF;*.AIFC;*.MID;*.RMI;*.AVI\0All files (*.*)\0*.*\0";
    of.lpstrFile = path;
    of.nMaxFile = size;
    of.lpstrTitle = "Open";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    path[0] = 0;
    ok = get_open(&of);
    FreeLibrary(dll);
    return ok ? 1 : 2;    /* 2: user cancelled; dialog itself was available */
}

static char dialog_spec[80];
static char dialog_result[260];

BOOL FAR PASCAL _export OpenDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        lstrcpy(dialog_spec, "*.*");
        SetDlgItemText(dlg, IDC_NAME, dialog_spec);
        DlgDirList(dlg, dialog_spec, IDC_FILES, IDC_PATH, 0x0000);
        DlgDirList(dlg, dialog_spec, IDC_DIRS, 0, 0x4010 | 0x8000);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDC_FILES && HIWORD(lp) == LBN_SELCHANGE) {
            char name[80];
            if (DlgDirSelect(dlg, name, IDC_FILES)) return TRUE;
            SetDlgItemText(dlg, IDC_NAME, name);
            return TRUE;
        }
        if ((wp == IDC_DIRS && HIWORD(lp) == LBN_DBLCLK) || (wp == IDC_FILES && HIWORD(lp) == LBN_DBLCLK) || wp == IDOK) {
            char name[80];
            if (wp == IDC_DIRS) {
                if (DlgDirSelect(dlg, name, IDC_DIRS)) {
                    lstrcat(name, dialog_spec);
                    DlgDirList(dlg, name, IDC_FILES, IDC_PATH, 0x0000);
                    DlgDirList(dlg, dialog_spec, IDC_DIRS, 0, 0x4010 | 0x8000);
                    SetDlgItemText(dlg, IDC_NAME, dialog_spec);
                }
                return TRUE;
            }
            GetDlgItemText(dlg, IDC_NAME, name, sizeof(name));
            if (!name[0]) return TRUE;
            {
                LPSTR c;
                BOOL wildcard = FALSE;
                for (c = name; *c; ++c) if (*c == '*' || *c == '?') wildcard = TRUE;
                if (wildcard) {
                    lstrcpy(dialog_spec, name);
                    DlgDirList(dlg, dialog_spec, IDC_FILES, IDC_PATH, 0x0000);
                    DlgDirList(dlg, dialog_spec, IDC_DIRS, 0, 0x4010 | 0x8000);
                    return TRUE;
                }
            }
            {
                OFSTRUCT of;
                if (OpenFile(name, &of, OF_EXIST) == HFILE_ERROR) {
                    MessageBox(dlg, "File not found.", "Open", MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
                lstrcpy(dialog_result, of.szPathName);
            }
            EndDialog(dlg, TRUE);
            return TRUE;
        }
        if (wp == IDCANCEL) { EndDialog(dlg, FALSE); return TRUE; }
        break;
    }
    return FALSE;
}

static void choose_file(void)
{
    char path[260];
    int result = commdlg_open(frame, path, sizeof(path));
    if (result == 1) { open_media(path); return; }
    if (result == 2) return;
    {
        FARPROC proc = MakeProcInstance((FARPROC)OpenDlgProc, instance);
        BOOL ok = DialogBox(instance, "OPENDLG", frame, (DLGPROC)proc);
        FreeProcInstance(proc);
        if (ok) open_media(dialog_result);
    }
}

/* -------------------------------------------------------------- windows */

BOOL FAR PASCAL _export AboutDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    if (msg == WM_INITDIALOG) {
        char text[160];
        WORD v = LOWORD(GetVersion());
        wsprintf(text, "Windows %d.%02d, %s mode. Output: %s.", LOBYTE(v), HIBYTE(v), mode_name(),
                 AoHasMmsystem() ? "MMSYSTEM waveOut or MCI" : "direct Sound Blaster");
        SetDlgItemText(dlg, IDC_ABOUTINFO, text);
        return TRUE;
    }
    if (msg == WM_COMMAND && (wp == IDOK || wp == IDCANCEL)) { EndDialog(dlg, TRUE); return TRUE; }
    return FALSE;
}

static void format_time(LPSTR out, DWORD ms)
{
    DWORD seconds = ms / 1000;
    wsprintf(out, "%lu:%02lu", seconds / 60, seconds % 60);
}

static void update_position(void)
{
    char text[260], now[16], total[16];
    DWORD position_ms = 0, length_ms = 0;
    if (use_mci) {
        char reply[32];
        mci("status rpmedia position", reply, sizeof(reply)); position_ms = (DWORD)parse_long(reply);
        mci("status rpmedia length", reply, sizeof(reply)); length_ms = (DWORD)parse_long(reply);
    } else if (have_stream) {
        PLAYBACK FAR *p;
        DWORD rate = 0, frames = 0;
        lock_playback(&p);
        if (p) { rate = p->stream.rate; frames = p->stream.total_frames; }
        unlock_playback();
        if (rate) {
            position_ms = muldiv(current_source_frame(), 1000, rate);
            length_ms = muldiv(frames, 1000, rate);
        }
    } else return;
    format_time(now, position_ms);
    format_time(total, length_ms);
    wsprintf(text, "%s / %s   %s", (LPSTR)now, (LPSTR)total, (LPSTR)info_text);
    SetWindowText(GetDlgItem(frame, IDC_POSITION), text);
}

LRESULT FAR PASCAL _export StatusProc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);
        RECT r;
        GetClientRect(window, &r);
        SetBkMode(dc, TRANSPARENT);
        DrawText(dc, status_text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        EndPaint(window, &ps);
        return 0;
    }
    return DefWindowProc(window, msg, wp, lp);
}

static void layout(HWND window)
{
    RECT r;
    GetClientRect(window, &r);
    MoveWindow(GetDlgItem(window, IDC_POSITION), 4, r.bottom - 44, r.right - 8, 20, TRUE);
    MoveWindow(status_window, 4, r.bottom - 22, r.right - 8, 20, TRUE);
}

LRESULT FAR PASCAL _export FrameProc(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10,
                     window, (HMENU)IDC_POSITION, instance, NULL);
        status_window = CreateWindow("RP16STATUS", "", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10,
                                     window, (HMENU)IDC_STATUS, instance, NULL);
        SetTimer(window, UI_TIMER_ID, 250, NULL);
        return 0;
    case WM_SIZE:
        layout(window);
        return 0;
    case WM_TIMER:
        if (wp == AO_TIMER_ID) AoService();
        else update_position();
        {
            AOSTATUS st;
            AoGetStatus(&st);
            if (have_stream && st.backend != AO_NONE && !st.playing && st.finished) {
                AoStop();
                set_status("Finished.");
            }
        }
        return 0;
    case MM_WOM_DONE:
        AoService();
        return 0;
    case MM_MCINOTIFY:
        if (wp == MCI_NOTIFY_SUCCESSFUL) set_status("Finished.");
        return 0;
    case WM_COMMAND:
        switch (wp) {
        case IDM_OPEN: choose_file(); break;
        case IDM_EXIT: DestroyWindow(window); break;
        case IDM_PLAY: play_media(); break;
        case IDM_PAUSE: pause_media(); break;
        case IDM_STOP: stop_media(); break;
        case IDM_BACK: seek_media(-10000L, FALSE); break;
        case IDM_FORWARD: seek_media(10000L, FALSE); break;
        case IDM_START: seek_media(0, TRUE); break;
        case IDM_ABOUT: {
            FARPROC proc = MakeProcInstance((FARPROC)AboutDlgProc, instance);
            DialogBox(instance, "ABOUTDLG", window, (DLGPROC)proc);
            FreeProcInstance(proc);
            break;
        }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, UI_TIMER_ID);
        close_media();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(window, msg, wp, lp);
}

/* Unattended self-test for guest automation: RETRO16 /test FILE [REPORT]
 * plays FILE to the end with the selected backend and writes a report. */
static BOOL self_test;
static char report_path[260];

static void write_report(LPCSTR result)
{
    char text[600];
    AOSTATUS st;
    HFILE f;
    WORD v = LOWORD(GetVersion());
    AoGetStatus(&st);
    wsprintf(text, "RetroPlayer 16 self-test\r\nWINDOWS=%d.%02d\r\nMODE=%s\r\nFILE=%s\r\n"
             "FORMAT=%s\r\nDEVICE=%s\r\nFRAMES_PLAYED=%lu\r\nSTATUS=%s\r\nRESULT=%s\r\n",
             LOBYTE(v), HIBYTE(v), mode_name(), (LPSTR)filename, (LPSTR)info_text,
             (LPSTR)st.device, st.frames_played, (LPSTR)status_text, result);
    f = _lcreat(report_path, 0);
    if (f != HFILE_ERROR) { _lwrite(f, text, lstrlen(text)); _lclose(f); }
}

static void parse_command(LPSTR cmd, LPSTR media)
{
    media[0] = 0;
    while (*cmd == ' ') ++cmd;
    if ((cmd[0] == '/' || cmd[0] == '-') && (cmd[1] | 0x20) == 't' && (cmd[2] | 0x20) == 'e' &&
        (cmd[3] | 0x20) == 's' && (cmd[4] | 0x20) == 't') {
        LPSTR p;
        self_test = TRUE;
        cmd += 5;
        while (*cmd == ' ') ++cmd;
        p = media;
        while (*cmd && *cmd != ' ') *p++ = *cmd++;
        *p = 0;
        while (*cmd == ' ') ++cmd;
        if (*cmd) lstrcpy(report_path, cmd);
        else lstrcpy(report_path, "C:\\RETROPLA\\REPORT16.TXT");
        return;
    }
    lstrcpy(media, cmd);
}

int PASCAL WinMain(HINSTANCE hi, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    MSG msg;
    char media[260];
    instance = hi;
    if (!prev) {
        _fmemset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = FrameProc;
        wc.hInstance = hi;
        wc.hIcon = LoadIcon(hi, "RETRO16");
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = "MAINMENU";
        wc.lpszClassName = "RP16FRAME";
        if (!RegisterClass(&wc)) return 1;
        wc.lpfnWndProc = StatusProc;
        wc.lpszMenuName = NULL;
        wc.hIcon = NULL;
        wc.lpszClassName = "RP16STATUS";
        if (!RegisterClass(&wc)) return 1;
    }
    lstrcpy(status_text, "Select File / Open.");
    frame = CreateWindow("RP16FRAME", "RetroPlayer 16", WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, 0, 480, 200, NULL, NULL, hi, NULL);
    if (!frame) return 1;
    ShowWindow(frame, show);
    UpdateWindow(frame);
    parse_command(cmd, media);
    if (media[0]) open_media(media);
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (self_test) {
            AOSTATUS st;
            AoGetStatus(&st);
            if (!lstrcmp(status_text, "Finished.") || (!have_stream && !use_mci)) {
                write_report(!lstrcmp(status_text, "Finished.") ? "PASS" : "FAIL");
                self_test = FALSE;
                DestroyWindow(frame);
            }
        }
    }
    return (int)msg.wParam;
}
