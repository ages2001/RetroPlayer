/* RetroPlayer 16 - audio output backends (see AOUT.H). */
#include <windows.h>
#include <mmsystem.h>
#include <conio.h>
#include "AOUT.H"
#include <string.h>


#ifdef RP16_TRACE
static void ao_trace(const char *msg)
{
    HFILE f = _lopen("C:\\RETROPLA\\RP16TRC.TXT", OF_READWRITE);
    if (f == HFILE_ERROR) f = _lcreat("C:\\RETROPLA\\RP16TRC.TXT", 0);
    if (f == HFILE_ERROR) return;
    _llseek(f, 0L, 2);
    _lwrite(f, msg, lstrlen(msg));
    _lwrite(f, "\r\n", 2);
    _lclose(f);
}
#else
#define ao_trace(m) ((void)0)
#endif

/* ---------------------------------------------------------------- common */

static int backend = AO_NONE;
static AOFORMAT format;
static AO_FILL fill_proc;
static void FAR *fill_context;
static HWND notify_window;
static BOOL playing, paused, finished;
static DWORD frames_played;
static char device_name[64];

static WORD frame_bytes(void) { return (WORD)(format.channels * (format.bits / 8)); }

/* --------------------------------------------------------------- waveOut */

typedef UINT (WINAPI *WO_GETNUM)(void);
typedef UINT (WINAPI *WO_OPEN)(HWAVEOUT FAR *, UINT, const WAVEFORMAT FAR *, DWORD, DWORD, DWORD);
typedef UINT (WINAPI *WO_HDR)(HWAVEOUT, WAVEHDR FAR *, UINT);
typedef UINT (WINAPI *WO_SIMPLE)(HWAVEOUT);
typedef UINT (WINAPI *WO_POS)(HWAVEOUT, MMTIME FAR *, UINT);
typedef UINT (WINAPI *WO_CAPS)(UINT, WAVEOUTCAPS FAR *, UINT);

static HINSTANCE mmsystem;
static WO_GETNUM wo_getnum;
static WO_OPEN wo_open;
static WO_HDR wo_prepare, wo_unprepare, wo_write;
static WO_SIMPLE wo_close, wo_reset, wo_pause, wo_restart;
static WO_POS wo_position;
static WO_CAPS wo_caps;

#define WO_BUFFERS 4
#define WO_BUFFER_BYTES 8192
static HWAVEOUT wave;
static HGLOBAL wo_data[WO_BUFFERS], wo_header[WO_BUFFERS];
static WAVEHDR FAR *wo_hdr[WO_BUFFERS];
static BOOL wo_queued[WO_BUFFERS];
static DWORD wo_base_frames;

static BOOL file_in_system(LPCSTR name)
{
    char path[160];
    OFSTRUCT of;
    int length = GetSystemDirectory(path, sizeof(path) - 16);
    if (length <= 0) return FALSE;
    if (path[length - 1] != '\\') path[length++] = '\\';
    lstrcpy(path + length, name);
    return OpenFile(path, &of, OF_EXIST) != HFILE_ERROR;
}

BOOL AoHasMmsystem(void)
{
    if (mmsystem) return TRUE;
    /* Windows 3.0 without the Multimedia Extensions has no MMSYSTEM.DLL;
     * checking first avoids the "Cannot find" message box of LoadLibrary. */
    if (!GetModuleHandle("MMSYSTEM") && !file_in_system("MMSYSTEM.DLL")) return FALSE;
    mmsystem = LoadLibrary("MMSYSTEM.DLL");
    if ((UINT)mmsystem < 32) { mmsystem = 0; return FALSE; }
    wo_getnum = (WO_GETNUM)GetProcAddress(mmsystem, "waveOutGetNumDevs");
    wo_open = (WO_OPEN)GetProcAddress(mmsystem, "waveOutOpen");
    wo_prepare = (WO_HDR)GetProcAddress(mmsystem, "waveOutPrepareHeader");
    wo_unprepare = (WO_HDR)GetProcAddress(mmsystem, "waveOutUnprepareHeader");
    wo_write = (WO_HDR)GetProcAddress(mmsystem, "waveOutWrite");
    wo_close = (WO_SIMPLE)GetProcAddress(mmsystem, "waveOutClose");
    wo_reset = (WO_SIMPLE)GetProcAddress(mmsystem, "waveOutReset");
    wo_pause = (WO_SIMPLE)GetProcAddress(mmsystem, "waveOutPause");
    wo_restart = (WO_SIMPLE)GetProcAddress(mmsystem, "waveOutRestart");
    wo_position = (WO_POS)GetProcAddress(mmsystem, "waveOutGetPosition");
    wo_caps = (WO_CAPS)GetProcAddress(mmsystem, "waveOutGetDevCaps");
    if (!wo_getnum || !wo_open || !wo_prepare || !wo_unprepare || !wo_write || !wo_close ||
        !wo_reset || !wo_pause || !wo_restart || !wo_position) {
        FreeLibrary(mmsystem); mmsystem = 0; return FALSE;
    }
    return TRUE;
}

static UINT wo_try(const AOFORMAT FAR *f, DWORD flags)
{
    PCMWAVEFORMAT pcm;
    pcm.wf.wFormatTag = WAVE_FORMAT_PCM;
    pcm.wf.nChannels = f->channels;
    pcm.wf.nSamplesPerSec = f->rate;
    pcm.wf.nBlockAlign = (WORD)(f->channels * (f->bits / 8));
    pcm.wf.nAvgBytesPerSec = f->rate * pcm.wf.nBlockAlign;
    pcm.wBitsPerSample = f->bits;
    return wo_open(&wave, (UINT)WAVE_MAPPER, (const WAVEFORMAT FAR *)&pcm,
                   (DWORD)(UINT)notify_window, 0, flags);
}

static BOOL wo_choose(const AOFORMAT FAR *wanted, AOFORMAT FAR *actual)
{
    static const DWORD rates[] = { 44100, 22050, 11025, 8000 };
    AOFORMAT f;
    int bits, channels, r;
    for (bits = wanted->bits; bits >= 8; bits -= 8)
        for (channels = wanted->channels; channels >= 1; --channels) {
            f.bits = (WORD)bits; f.channels = (WORD)channels; f.rate = wanted->rate;
            if (wo_try(&f, WAVE_FORMAT_QUERY) == 0) { *actual = f; return TRUE; }
            for (r = 0; r < 4; ++r) {
                f.rate = rates[r];
                if (f.rate > wanted->rate && r < 3 && rates[r + 1] >= wanted->rate) continue;
                if (wo_try(&f, WAVE_FORMAT_QUERY) == 0) { *actual = f; return TRUE; }
            }
        }
    return FALSE;
}

static void wo_free(void)
{
    int i;
    for (i = 0; i < WO_BUFFERS; ++i) {
        if (wo_hdr[i]) {
            if (wave) wo_unprepare(wave, wo_hdr[i], sizeof(WAVEHDR));
            if (wo_hdr[i]->lpData) GlobalUnlock(wo_data[i]);
            GlobalUnlock(wo_header[i]);
        }
        if (wo_data[i]) GlobalFree(wo_data[i]);
        if (wo_header[i]) GlobalFree(wo_header[i]);
        wo_data[i] = wo_header[i] = 0; wo_hdr[i] = NULL; wo_queued[i] = FALSE;
    }
}

static BOOL wo_alloc(void)
{
    int i;
    for (i = 0; i < WO_BUFFERS; ++i) {
        wo_data[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, WO_BUFFER_BYTES);
        wo_header[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, sizeof(WAVEHDR));
        if (!wo_data[i] || !wo_header[i]) return FALSE;
        wo_hdr[i] = (WAVEHDR FAR *)GlobalLock(wo_header[i]);
        wo_hdr[i]->lpData = (LPSTR)GlobalLock(wo_data[i]);
        wo_hdr[i]->dwBufferLength = WO_BUFFER_BYTES;
        if (!wo_hdr[i]->lpData || wo_prepare(wave, wo_hdr[i], sizeof(WAVEHDR))) return FALSE;
    }
    return TRUE;
}

static void wo_feed(void)
{
    int i;
    WORD max_frames = (WORD)(WO_BUFFER_BYTES / frame_bytes());
    for (i = 0; i < WO_BUFFERS && !finished; ++i) {
        WORD frames;
        if (wo_queued[i] && !(wo_hdr[i]->dwFlags & WHDR_DONE)) continue;
        wo_queued[i] = FALSE;
        frames = fill_proc(fill_context, (BYTE FAR *)wo_hdr[i]->lpData, max_frames);
        if (!frames) { finished = TRUE; break; }
        wo_hdr[i]->dwBufferLength = (DWORD)frames * frame_bytes();
        wo_hdr[i]->dwFlags &= ~WHDR_DONE;
        if (wo_write(wave, wo_hdr[i], sizeof(WAVEHDR)) == 0) wo_queued[i] = TRUE;
        else { finished = TRUE; break; }
    }
}

static BOOL wo_all_done(void)
{
    int i;
    for (i = 0; i < WO_BUFFERS; ++i) if (wo_queued[i] && !(wo_hdr[i]->dwFlags & WHDR_DONE)) return FALSE;
    return TRUE;
}

/* ------------------------------------------------------ Sound Blaster DSP */

#define SB_BUFFER_BYTES 16384U    /* two halves of 8 KiB */
static WORD sb_base = 0x220, sb_dma = 1, sb_irq = 5;
static WORD sb_version;
static DWORD sb_dos_block;        /* GlobalDosAlloc result */
static BYTE FAR *sb_buffer;       /* protected-mode pointer to the DMA buffer */
static DWORD sb_physical;
static int sb_half_filled[2];
static int sb_last_half;
static WORD sb_end_half_bytes = 0xFFFF; /* bytes valid in the final half */
static int sb_end_half = -1;
static BOOL sb_single;      /* enhanced mode: single-cycle DMA, reprogram/block */
static int sb_cur_half;     /* half currently playing (single-cycle) */
static DWORD sb_block_start; /* GetTickCount when the current block began */
static DWORD sb_block_ms;    /* nominal duration of one half block, ms */

static const BYTE dma_page_port[4] = { 0x87, 0x83, 0x81, 0x82 };

/* --- Virtual DMA Services (386 enhanced mode) ------------------------------
 * In enhanced mode paging is active, so a conventional-memory segment's
 * paragraph address is NOT its physical address, and the pages may move.
 * VDS (INT 4Bh) locks the region and returns its true physical address;
 * VDMAD then translates the direct DMA-controller port writes correctly. */
typedef struct {
    DWORD region_size;
    DWORD offset;
    WORD  seg_or_sel;
    WORD  buffer_id;
    DWORD physical;
} VDS_DDS;
static VDS_DDS vds_dds;
static BOOL sb_vds_locked;

static int vds_int(unsigned ax, unsigned dx, void FAR *dds);
#pragma aux vds_int =              \
    "int 4bh"                      \
    "sbb ax, ax"                   \
    parm [ax] [dx] [es di]         \
    value [ax]                     \
    modify exact [ax bx cx dx si di es];

static BOOL vds_present(void)
{
    /* Get Version (AX=8102h): carry clear means VDS is available. */
    return vds_int(0x8102, 0, (void FAR *)&vds_dds) == 0;
}



static void sb_delay(void)
{
    int i;
    for (i = 0; i < 16; ++i) (void)inp(0x80);
}

static BOOL sb_write(BYTE value)
{
    DWORD start = GetTickCount();
    while (inp(sb_base + 0x0C) & 0x80)
        if (GetTickCount() - start > 50) return FALSE;
    outp(sb_base + 0x0C, value);
    return TRUE;
}

static int sb_read(void)
{
    DWORD start = GetTickCount();
    while (!(inp(sb_base + 0x0E) & 0x80))
        if (GetTickCount() - start > 50) return -1;
    return inp(sb_base + 0x0A);
}

static BOOL sb_reset(void)
{
    int tries;
    outp(sb_base + 0x06, 1);
    sb_delay(); sb_delay();
    outp(sb_base + 0x06, 0);
    for (tries = 0; tries < 8; ++tries) if (sb_read() == 0xAA) return TRUE;
    return FALSE;
}

static WORD hex_value(LPCSTR p) {
    WORD v = 0;
    for (;; ++p) {
        if (*p >= '0' && *p <= '9') v = (WORD)(v * 16 + *p - '0');
        else if (*p >= 'A' && *p <= 'F') v = (WORD)(v * 16 + *p - 'A' + 10);
        else if (*p >= 'a' && *p <= 'f') v = (WORD)(v * 16 + *p - 'a' + 10);
        else return v;
    }
}
static WORD dec_value(LPCSTR p) { WORD v = 0; while (*p >= '0' && *p <= '9') v = (WORD)(v * 10 + *p++ - '0'); return v; }

static void sb_parse_blaster(void)
{
    LPSTR env = GetDOSEnvironment();
    while (env && *env) {
        if (!_fstrnicmp(env, "BLASTER=", 8)) {
            LPSTR p = env + 8;
            while (*p) {
                char key = (char)(*p & ~0x20);
                if (key == 'A') sb_base = hex_value(p + 1);
                else if (key == 'D') sb_dma = dec_value(p + 1);
                else if (key == 'I') sb_irq = dec_value(p + 1);
                while (*p && *p != ' ') ++p;
                while (*p == ' ') ++p;
            }
            break;
        }
        env += lstrlen(env) + 1;
    }
    if (sb_dma > 3) sb_dma = 1;
}

static void sb_free(void)
{
    if (sb_vds_locked) { vds_int(0x8104, 0, (void FAR *)&vds_dds); sb_vds_locked = FALSE; }
    if (sb_dos_block) GlobalDosFree(LOWORD(sb_dos_block));
    sb_dos_block = 0; sb_buffer = NULL;
}

static BOOL sb_alloc(void)
{
    DWORD physical;
    WORD offset;
    /* Twice the size so a part never crosses a 64 KiB DMA page. */
    sb_dos_block = GlobalDosAlloc(2UL * SB_BUFFER_BYTES);
    if (!sb_dos_block) return FALSE;
    sb_single = FALSE;
    physical = (DWORD)HIWORD(sb_dos_block) << 4;
    offset = 0;
    if ((physical & 0xFFFFUL) + SB_BUFFER_BYTES > 0x10000UL)
        offset = (WORD)(0x10000UL - (physical & 0xFFFFUL));
    sb_buffer = (BYTE FAR *)MAKELP(LOWORD(sb_dos_block), offset);
    sb_vds_locked = FALSE;
    if (GetWinFlags() & WF_ENHANCED) {
        if (!vds_present()) { sb_free(); return FALSE; }
        vds_dds.region_size = SB_BUFFER_BYTES;
        vds_dds.offset = offset;
        vds_dds.seg_or_sel = LOWORD(sb_dos_block);
        vds_dds.buffer_id = 0;
        vds_dds.physical = 0;
        /* flags 0x0C: no copy buffer, no 64 KiB-boundary crossing. */
        if (vds_int(0x8103, 0x000C, (void FAR *)&vds_dds)) { sb_free(); return FALSE; }
        if (vds_dds.buffer_id != 0) {           /* needed a copy buffer: reject */
            vds_int(0x8104, 0, (void FAR *)&vds_dds);
            sb_free(); return FALSE;
        }
        /* VDMAD translates the port writes, so program the controller with the
         * app-visible (segment<<4) address, not the VDS physical one. The VDS
         * lock keeps the region from moving while the single-cycle DMA runs. */
        sb_physical = physical + offset;
        sb_vds_locked = TRUE;
        sb_single = TRUE;   /* VDMAD cannot virtualize auto-init DMA on Win3.0 */
    } else {
        sb_physical = physical + offset;        /* real/standard: no paging */
    }
    return TRUE;
}

/* Program the 8237 DMA controller. auto_init=TRUE sets continuous (auto-init)
 * mode over the whole buffer; FALSE sets single-cycle over one half at base
 * sb_physical + half*half. Count is in bytes-1 for 8-bit DMA. */
static void dma_setup(BOOL auto_init, int half, WORD bytes)
{
    WORD c = sb_dma, count = (WORD)(bytes - 1);
    DWORD base = sb_physical + (DWORD)(auto_init ? 0 : half) * (SB_BUFFER_BYTES / 2);
    BYTE mode = (BYTE)((auto_init ? 0x58 : 0x48) | c);  /* read from memory */
    outp(0x0A, (BYTE)(0x04 | c));               /* mask channel */
    outp(0x0C, 0);                              /* clear byte-pointer flip-flop */
    outp(0x0B, mode);
    outp((WORD)(c * 2), (BYTE)base);
    outp((WORD)(c * 2), (BYTE)(base >> 8));
    outp(dma_page_port[c], (BYTE)(base >> 16));
    outp(0x0C, 0);
    outp((WORD)(c * 2 + 1), (BYTE)count);
    outp((WORD)(c * 2 + 1), (BYTE)(count >> 8));
    outp(0x0A, (BYTE)c);                        /* unmask channel */
}

static WORD dma_remaining(void)
{
    WORD lo, hi, lo2, hi2;
    do {
        outp(0x0C, 0);
        lo = inp(sb_dma * 2 + 1); hi = inp(sb_dma * 2 + 1);
        outp(0x0C, 0);
        lo2 = inp(sb_dma * 2 + 1); hi2 = inp(sb_dma * 2 + 1);
    } while (hi != hi2);
    (void)lo2;
    return (WORD)(((hi << 8) | lo) + 1);
}

static void sb_fill_half(int half)
{
    WORD half_bytes = SB_BUFFER_BYTES / 2, max_frames = (WORD)(half_bytes / frame_bytes()), frames = 0;
    BYTE FAR *dst = sb_buffer + half * half_bytes;
    if (!finished) frames = fill_proc(fill_context, dst, max_frames);
    if (frames < max_frames) {
        /* Silence (unsigned 8-bit) after the end. */
        _fmemset(dst + frames * frame_bytes(), 0x80, half_bytes - frames * frame_bytes());
        if (!finished) { finished = TRUE; sb_end_half = half; sb_end_half_bytes = (WORD)(frames * frame_bytes()); }
    }
    sb_half_filled[half] = 1;
}

/* Program the sample rate once (SB16 uses an exact rate; older DSPs a time
 * constant). Called before the first block. */
static void sb_set_rate(void)
{
    if (sb_version >= 0x400) {
        sb_write(0x41); sb_write((BYTE)(format.rate >> 8)); sb_write((BYTE)format.rate);
    } else {
        WORD tc = (WORD)(256 - 1000000L / format.rate);
        sb_write(0x40); sb_write((BYTE)tc);
    }
}

/* Start one DSP transfer of (samples) frames. auto_init selects continuous
 * mode (the whole buffer, DMA wraps); otherwise a single block. */
static void sb_start_dsp(BOOL auto_init, WORD samples)
{
    WORD count = (WORD)(samples - 1);
    if (sb_version >= 0x400) {
        sb_write((BYTE)(auto_init ? 0xC6 : 0xC0));        /* 8-bit out, FIFO */
        sb_write((BYTE)(format.channels == 2 ? 0x20 : 0x00)); /* unsigned mono/stereo */
        sb_write((BYTE)count); sb_write((BYTE)(count >> 8));
    } else {
        sb_write((BYTE)(0x48)); sb_write((BYTE)count); sb_write((BYTE)(count >> 8));
        sb_write((BYTE)(auto_init ? 0x1C : 0x14));        /* 8-bit auto-init / single */
    }
}

/* Begin playing the given half with a fresh single-cycle transfer. */
static void sb_play_block(int half)
{
    WORD half_samples = (WORD)((SB_BUFFER_BYTES / 2) / frame_bytes());
    dma_setup(FALSE, half, (WORD)(SB_BUFFER_BYTES / 2));
    sb_start_dsp(FALSE, half_samples);
    sb_cur_half = half;
    /* VDMAD does not update the virtual DMA count in enhanced mode, so time
     * the block instead of reading the controller. */
    sb_block_ms = (DWORD)half_samples * 1000UL / format.rate;
    sb_block_start = GetTickCount();
}

static void sb_halt(void)
{
    if (sb_version >= 0x200) sb_write(0xDA); else sb_write(0xD0);
    sb_write(0xD0);
    outp(0x0A, 0x04 | sb_dma);
    (void)inp(sb_base + 0x0E);
}

static void sb_service(void)
{
    WORD half_bytes = SB_BUFFER_BYTES / 2;
    if (!playing || paused) return;
    (void)inp(sb_base + 0x0E);                    /* acknowledge a pending 8-bit IRQ */

    if (sb_single) {
        /* Single-cycle: the block ends after its nominal duration. */
        if (GetTickCount() - sb_block_start < sb_block_ms)
            return;                               /* block still playing */
        if (sb_cur_half == sb_end_half) {
            frames_played += sb_end_half_bytes / frame_bytes();
            sb_halt(); playing = FALSE;
            PostMessage(notify_window, MM_WOM_DONE, 0, 0L);
            return;
        }
        frames_played += half_bytes / frame_bytes();
        {
            int done = sb_cur_half, next = sb_cur_half ^ 1;
            sb_play_block(next);                  /* start the pre-filled half */
            if (!finished || sb_end_half < 0) sb_fill_half(done);  /* refill behind */
            else _fmemset(sb_buffer + done * half_bytes, 0x80, half_bytes);
        }
        return;
    }

    /* Auto-init: detect which half the controller is now reading. */
    {
        WORD position = (WORD)(SB_BUFFER_BYTES - dma_remaining());
        int current = position >= half_bytes;
        if (current != sb_last_half) {
            int done = sb_last_half;
            if (done == sb_end_half) {
                frames_played += sb_end_half_bytes / frame_bytes();
                sb_halt(); playing = FALSE;
                PostMessage(notify_window, MM_WOM_DONE, 0, 0L);
                return;
            }
            frames_played += half_bytes / frame_bytes();
            sb_half_filled[done] = 0;
            sb_last_half = current;
            if (!finished || sb_end_half < 0) sb_fill_half(done);
            else _fmemset(sb_buffer + done * half_bytes, 0x80, half_bytes);
        }
    }
}

/* ------------------------------------------------------------ public API */

BOOL AoOpen(HWND notify, const AOFORMAT FAR *wanted, AOFORMAT FAR *actual,
            AO_FILL fill, void FAR *context, LPSTR error)
{
    AoStop();
    notify_window = notify; fill_proc = fill; fill_context = context;
    playing = paused = finished = FALSE; frames_played = 0;
    if (AoHasMmsystem() && wo_getnum() > 0) {
        if (!wo_choose(wanted, &format)) { lstrcpy(error, "The wave device rejects every PCM format."); return FALSE; }
        if (wo_try(&format, CALLBACK_WINDOW)) { lstrcpy(error, "The wave device could not be opened."); wave = 0; return FALSE; }
        if (!wo_alloc()) { wo_free(); wo_close(wave); wave = 0; lstrcpy(error, "Out of memory for audio buffers."); return FALSE; }
        backend = AO_WAVEOUT;
        lstrcpy(device_name, "MMSYSTEM waveOut");
        if (wo_caps) {
            WAVEOUTCAPS caps;
            if (!wo_caps(0, &caps, sizeof(caps))) { lstrcpy(device_name, "waveOut: "); lstrcat(device_name, caps.szPname); }
        }
        wo_base_frames = 0;
        *actual = format;
        return TRUE;
    }
    ao_trace("AoOpen: no waveout, trying SB");
    sb_parse_blaster();
    ao_trace("AoOpen: blaster parsed");
    if (!sb_reset()) { lstrcpy(error, "No wave driver and no Sound Blaster DSP found (check BLASTER)."); return FALSE; }
    ao_trace("AoOpen: sb_reset ok");
    sb_write(0xE1);
    sb_version = (WORD)((sb_read() & 0xFF) << 8); sb_version |= (WORD)(sb_read() & 0xFF);
    if (sb_version < 0x200) { lstrcpy(error, "Sound Blaster DSP 2.0 or later is required."); return FALSE; }
    format.bits = 8;
    format.channels = (WORD)(sb_version >= 0x400 && wanted->channels > 1 ? 2 : 1);
    format.rate = wanted->rate;
    if (sb_version < 0x400 && format.rate > 22050) format.rate = 22050;
    if (format.rate > 44100) format.rate = 44100;
    if (format.rate < 4000) format.rate = 4000;
    ao_trace("AoOpen: dsp version read");
    if (!sb_alloc()) { lstrcpy(error, "No conventional memory for the DMA buffer."); return FALSE; }
    ao_trace("AoOpen: sb_alloc ok");
    backend = AO_BLASTER;
    wsprintf(device_name, "Sound Blaster DSP %d.%02d at %Xh DMA %d", sb_version >> 8, sb_version & 0xFF, sb_base, sb_dma);
    *actual = format;
    return TRUE;
}

BOOL AoStart(void)
{
    if (backend == AO_WAVEOUT) {
        playing = TRUE; finished = FALSE;
        wo_pause(wave);
        wo_feed();
        wo_restart(wave);
        return TRUE;
    }
    if (backend == AO_BLASTER) {
        WORD half_samples = (WORD)((SB_BUFFER_BYTES / 2) / frame_bytes());
        ao_trace("AoStart: SB begin");
        finished = FALSE; sb_end_half = -1;
        sb_fill_half(0); sb_fill_half(1);
        ao_trace("AoStart: halves filled");
        sb_last_half = 0;
        sb_write(0xD1);                       /* speaker on */
        ao_trace("AoStart: speaker on");
        sb_set_rate();
        if (sb_single) {
            sb_play_block(0);                 /* single-cycle: first half only */
        } else {
            dma_setup(TRUE, 0, (WORD)SB_BUFFER_BYTES);
            sb_start_dsp(TRUE, half_samples); /* auto-init over both halves */
        }
        ao_trace("AoStart: dsp started");
        playing = TRUE;
        SetTimer(notify_window, AO_TIMER_ID, 20, NULL);
        return TRUE;
    }
    return FALSE;
}

void AoPause(BOOL pause)
{
    if (!playing || pause == paused) return;
    paused = pause;
    if (backend == AO_WAVEOUT) { if (pause) wo_pause(wave); else wo_restart(wave); }
    else if (backend == AO_BLASTER) {
        sb_write((BYTE)(pause ? 0xD0 : 0xD4));   /* halt / continue 8-bit DMA */
    }
}

void AoReset(void)
{
    if (backend == AO_WAVEOUT && wave) {
        wo_reset(wave);
        { int i; for (i = 0; i < WO_BUFFERS; ++i) wo_queued[i] = FALSE; }
        wo_base_frames = 0;
    } else if (backend == AO_BLASTER && playing) {
        sb_halt();
        KillTimer(notify_window, AO_TIMER_ID);
    }
    playing = paused = finished = FALSE;
    frames_played = 0;   /* positions restart at the caller's seek base */
}

void AoStop(void)
{
    AoReset();
    if (backend == AO_WAVEOUT) { wo_free(); if (wave) wo_close(wave); wave = 0; }
    else if (backend == AO_BLASTER) { sb_write(0xD3); sb_free(); }
    backend = AO_NONE;
    frames_played = 0;
}

void AoService(void)
{
    if (backend == AO_WAVEOUT && playing) {
        wo_feed();
        if (finished && wo_all_done()) playing = FALSE;
    } else if (backend == AO_BLASTER) sb_service();
}

void AoGetStatus(AOSTATUS FAR *st)
{
    st->backend = backend;
    st->playing = playing;
    st->paused = paused;
    st->finished = finished && !playing;
    st->frames_played = frames_played;
    if (backend == AO_WAVEOUT && wave) {
        MMTIME t;
        t.wType = TIME_SAMPLES;
        if (!wo_position(wave, &t, sizeof(t)) && t.wType == TIME_SAMPLES) st->frames_played = wo_base_frames + t.u.sample;
    }
    lstrcpy(st->device, device_name);
}
