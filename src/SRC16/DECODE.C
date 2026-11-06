/* RetroPlayer 16 - container parsing and software audio decoding.
 * Written for 16-bit compilers (int is 16 bits); also compiles on a host
 * C compiler for regression tests (RP16_HOST, see TESTS16/HOSTDEC.C). */
#include "DECODE.H"
#include <string.h>

#define WAVE_PCM        0x0001
#define WAVE_MSADPCM    0x0002
#define WAVE_ALAW       0x0006
#define WAVE_ULAW       0x0007
#define WAVE_IMAADPCM   0x0011
#define WAVE_EXTENSIBLE 0xFFFE

static WORD le16(const BYTE FAR *p) { return (WORD)(p[0] | ((WORD)p[1] << 8)); }
static DWORD le32(const BYTE FAR *p) { return (DWORD)le16(p) | ((DWORD)le16(p + 2) << 16); }
static WORD be16(const BYTE FAR *p) { return (WORD)(((WORD)p[0] << 8) | p[1]); }
static DWORD be32(const BYTE FAR *p) { return ((DWORD)be16(p) << 16) | be16(p + 2); }

static void set_error(RPSTREAM FAR *s, LPCSTR text) { lstrcpy(s->error, text); }

static BOOL read_exact(RPSTREAM FAR *s, void FAR *buffer, WORD count)
{
    return _lread(s->file, buffer, count) == count;
}

static BOOL seek_to(RPSTREAM FAR *s, DWORD offset)
{
    return _llseek(s->file, (LONG)offset, 0) == (LONG)offset;
}

short RpUlaw(BYTE v)
{
    int sign, exponent, mantissa, sample;
    v = (BYTE)~v;
    sign = v & 0x80;
    exponent = (v >> 4) & 7;
    mantissa = v & 15;
    sample = ((mantissa << 3) + 0x84) << exponent;
    sample -= 0x84;
    return (short)(sign ? -sample : sample);
}

short RpAlaw(BYTE v)
{
    int sign, exponent, mantissa, sample;
    v ^= 0x55;
    sign = v & 0x80;
    exponent = (v >> 4) & 7;
    mantissa = v & 15;
    if (exponent == 0) sample = (mantissa << 4) + 8;
    else sample = ((mantissa << 4) + 0x108) << (exponent - 1);
    return (short)(sign ? sample : -sample);
}

/* IEEE 754 80-bit extended (AIFF sample rate) to an integer rate. */
DWORD RpExtendedRate(const BYTE FAR *e)
{
    int exponent = (int)(((e[0] & 0x7F) << 8) | e[1]) - 16383;
    DWORD mantissa = be32(e + 2);
    if (e[0] & 0x80) return 0;
    if (exponent < 0) return 0;
    if (exponent > 31) return 0;
    return mantissa >> (31 - exponent);
}

static BOOL check_format(RPSTREAM FAR *s)
{
    if (s->channels < 1 || s->channels > RP_MAX_CHANNELS) {
        set_error(s, "Only mono and stereo streams are supported."); return FALSE;
    }
    if (s->rate < 1000 || s->rate > 96000) { set_error(s, "Unsupported sample rate."); return FALSE; }
    return TRUE;
}

static void pcm_layout(RPSTREAM FAR *s)
{
    WORD bytes = (WORD)((s->codec == RPC_PCM_S16LE || s->codec == RPC_PCM_S16BE) ? 2 : 1);
    s->block_align = (WORD)(bytes * s->channels);
    s->block_frames = 1;
    s->total_frames = s->data_bytes / s->block_align;
}

static BOOL open_wave(RPSTREAM FAR *s)
{
    BYTE head[12], chunk[8], fmt[64];
    DWORD offset = 12, size;
    BOOL have_fmt = FALSE;
    WORD tag = 0, i;
    if (!seek_to(s, 0) || !read_exact(s, head, 12)) return FALSE;
    for (;;) {
        if (!seek_to(s, offset) || !read_exact(s, chunk, 8)) {
            set_error(s, "WAVE file has no data chunk."); return FALSE;
        }
        size = le32(chunk + 4);
        if (!_fmemcmp(chunk, "fmt ", 4)) {
            WORD n = (WORD)(size < sizeof(fmt) ? size : sizeof(fmt));
            if (size < 16 || !read_exact(s, fmt, n)) { set_error(s, "Bad WAVE format chunk."); return FALSE; }
            tag = le16(fmt);
            s->channels = le16(fmt + 2);
            s->rate = le32(fmt + 4);
            s->block_align = le16(fmt + 12);
            s->bits = le16(fmt + 14);
            if (tag == WAVE_EXTENSIBLE && n >= 40) tag = le16(fmt + 24);
            if (tag == WAVE_PCM) {
                if (s->bits == 8) s->codec = RPC_PCM_U8;
                else if (s->bits == 16) s->codec = RPC_PCM_S16LE;
                else { set_error(s, "Only 8-bit and 16-bit PCM WAVE is supported."); return FALSE; }
            } else if (tag == WAVE_ULAW) s->codec = RPC_ULAW;
            else if (tag == WAVE_ALAW) s->codec = RPC_ALAW;
            else if (tag == WAVE_IMAADPCM) {
                s->codec = RPC_IMA_ADPCM;
                if (s->bits != 4 || s->block_align < 4 * s->channels) {
                    set_error(s, "Unsupported IMA ADPCM layout."); return FALSE;
                }
                s->block_frames = (WORD)(((s->block_align - 4 * s->channels) * 8) / (4 * s->channels) + 1);
            } else if (tag == WAVE_MSADPCM) {
                s->codec = RPC_MS_ADPCM;
                if (n < 22) { set_error(s, "Bad Microsoft ADPCM header."); return FALSE; }
                s->block_frames = le16(fmt + 18);
                s->ms_ncoef = le16(fmt + 20);
                if (s->ms_ncoef < 7 || s->ms_ncoef > 32 || n < (WORD)(22 + 4 * s->ms_ncoef)) {
                    set_error(s, "Bad Microsoft ADPCM coefficients."); return FALSE;
                }
                for (i = 0; i < s->ms_ncoef; ++i) {
                    s->ms_coef1[i] = (short)le16(fmt + 22 + 4 * i);
                    s->ms_coef2[i] = (short)le16(fmt + 24 + 4 * i);
                }
            } else {
                set_error(s, "This WAVE compression is not supported."); return FALSE;
            }
            have_fmt = TRUE;
        } else if (!_fmemcmp(chunk, "data", 4)) {
            if (!have_fmt) { set_error(s, "WAVE data precedes its format."); return FALSE; }
            s->data_start = offset + 8;
            s->data_bytes = size;
            break;
        }
        offset += 8 + size + (size & 1);
    }
    if (!check_format(s)) return FALSE;
    if (s->codec == RPC_IMA_ADPCM || s->codec == RPC_MS_ADPCM) {
        if (s->block_frames < 2 || s->block_frames > 2041) { set_error(s, "Bad ADPCM block size."); return FALSE; }
        s->total_frames = (s->data_bytes / s->block_align) * s->block_frames;
    } else pcm_layout(s);
    return TRUE;
}

/* VOC: positions the stream at the next sound data block. */
static BOOL voc_next(RPSTREAM FAR *s, DWORD offset)
{
    BYTE b[16];
    DWORD size;
    for (;;) {
        if (!seek_to(s, offset) || !read_exact(s, b, 1) || b[0] == 0) { s->voc_more = FALSE; return FALSE; }
        if (!read_exact(s, b + 1, 3)) { s->voc_more = FALSE; return FALSE; }
        size = (DWORD)b[1] | ((DWORD)b[2] << 8) | ((DWORD)b[3] << 16);
        if (b[0] == 1) {
            if (size < 2 || !read_exact(s, b + 4, 2)) return FALSE;
            if (b[5] != 0) { set_error(s, "Packed VOC data is not supported."); return FALSE; }
            if (!s->voc_extended && !s->rate) { s->rate = 1000000L / (256 - b[4]); s->channels = 1; s->codec = RPC_PCM_U8; }
            s->voc_extended = FALSE;
            s->data_start = offset + 6; s->data_bytes = size - 2;
        } else if (b[0] == 8) {
            /* Extended block: applies to the following type 1 block. */
            WORD tc;
            if (size < 4 || !read_exact(s, b + 4, 4)) return FALSE;
            if (b[6] != 0) { set_error(s, "Packed VOC data is not supported."); return FALSE; }
            tc = le16(b + 4);
            s->channels = (WORD)(b[7] ? 2 : 1);
            s->rate = 256000000L / (65536L - tc) / s->channels;
            s->codec = RPC_PCM_U8;
            s->voc_extended = TRUE;
            offset += 4 + size;
            continue;
        } else if (b[0] == 2) {
            s->data_start = offset + 4; s->data_bytes = size;
        } else if (b[0] == 9) {
            WORD codec;
            if (size < 12 || !read_exact(s, b + 4, 12)) return FALSE;
            s->rate = le32(b + 4); s->bits = b[8]; s->channels = b[9]; codec = le16(b + 10);
            if (codec == 0 && s->bits == 8) s->codec = RPC_PCM_U8;
            else if (codec == 4 && s->bits == 16) s->codec = RPC_PCM_S16LE;
            else if (codec == 6) s->codec = RPC_ALAW;
            else if (codec == 7) s->codec = RPC_ULAW;
            else { set_error(s, "Unsupported VOC codec."); return FALSE; }
            s->data_start = offset + 16; s->data_bytes = size - 12;
        } else {
            offset += 4 + size;
            continue;
        }
        s->data_pos = 0;
        s->voc_more = TRUE;
        return seek_to(s, s->data_start);
    }
}

static BOOL open_voc(RPSTREAM FAR *s)
{
    BYTE head[26];
    if (!seek_to(s, 0) || !read_exact(s, head, 26)) return FALSE;
    if (!voc_next(s, le16(head + 20))) { if (!s->error[0]) set_error(s, "VOC file has no sound data."); return FALSE; }
    if (!check_format(s)) return FALSE;
    pcm_layout(s);
    s->total_frames = 0; /* unknown across several blocks */
    return TRUE;
}

static BOOL open_au(RPSTREAM FAR *s)
{
    BYTE h[24];
    DWORD encoding;
    if (!seek_to(s, 0) || !read_exact(s, h, 24)) return FALSE;
    s->data_start = be32(h + 4);
    s->data_bytes = be32(h + 8);
    encoding = be32(h + 12);
    s->rate = be32(h + 16);
    s->channels = (WORD)be32(h + 20);
    if (encoding == 1) s->codec = RPC_ULAW;
    else if (encoding == 2) s->codec = RPC_PCM_S8;
    else if (encoding == 3) s->codec = RPC_PCM_S16BE;
    else if (encoding == 27) s->codec = RPC_ALAW;
    else { set_error(s, "Unsupported AU encoding."); return FALSE; }
    if (s->data_bytes == 0xFFFFFFFFUL) {
        LONG end = _llseek(s->file, 0, 2);
        s->data_bytes = end > (LONG)s->data_start ? (DWORD)end - s->data_start : 0;
    }
    if (!check_format(s)) return FALSE;
    pcm_layout(s);
    return TRUE;
}

static BOOL open_aiff(RPSTREAM FAR *s, BOOL aifc)
{
    BYTE chunk[8], comm[26];
    DWORD offset = 12, size;
    BOOL have_comm = FALSE;
    for (;;) {
        if (!seek_to(s, offset) || !read_exact(s, chunk, 8)) { set_error(s, "AIFF file has no sound data."); return FALSE; }
        size = be32(chunk + 4);
        if (!_fmemcmp(chunk, "COMM", 4)) {
            if (size < 18 || !read_exact(s, comm, (WORD)(size < 26 ? size : 26))) return FALSE;
            s->channels = be16(comm);
            s->bits = be16(comm + 6);
            s->rate = RpExtendedRate(comm + 8);
            s->codec = s->bits == 8 ? RPC_PCM_S8 : s->bits == 16 ? RPC_PCM_S16BE : 0;
            if (aifc && size >= 22) {
                if (!_fmemcmp(comm + 18, "sowt", 4) && s->bits == 16) s->codec = RPC_PCM_S16LE;
                else if (!_fmemcmp(comm + 18, "ulaw", 4)) s->codec = RPC_ULAW;
                else if (!_fmemcmp(comm + 18, "alaw", 4)) s->codec = RPC_ALAW;
                else if (_fmemcmp(comm + 18, "NONE", 4)) s->codec = 0;
            }
            if (!s->codec) { set_error(s, "Unsupported AIFF sample format."); return FALSE; }
            have_comm = TRUE;
        } else if (!_fmemcmp(chunk, "SSND", 4)) {
            BYTE so[8];
            if (!have_comm || size < 8 || !read_exact(s, so, 8)) { set_error(s, "Bad AIFF sound chunk."); return FALSE; }
            s->data_start = offset + 16 + be32(so);
            s->data_bytes = size - 8 - be32(so);
            break;
        }
        offset += 8 + size + (size & 1);
    }
    if (!check_format(s)) return FALSE;
    pcm_layout(s);
    return TRUE;
}

BOOL RpOpen(RPSTREAM FAR *s, LPCSTR path)
{
    BYTE head[12];
    BOOL ok = FALSE;
    _fmemset(s, 0, sizeof(*s));
    s->file = _lopen(path, OF_READ | OF_SHARE_DENY_WRITE);
    if (s->file == HFILE_ERROR) { set_error(s, "Cannot open the file."); return FALSE; }
    if (!read_exact(s, head, 12)) { set_error(s, "File is too short."); }
    else if (!_fmemcmp(head, "RIFF", 4) && !_fmemcmp(head + 8, "WAVE", 4)) { s->container = RPF_WAVE; ok = open_wave(s); }
    else if (!_fmemcmp(head, "Creative V", 10)) { s->container = RPF_VOC; ok = open_voc(s); }
    else if (!_fmemcmp(head, ".snd", 4)) { s->container = RPF_AU; ok = open_au(s); }
    else if (!_fmemcmp(head, "FORM", 4) && (!_fmemcmp(head + 8, "AIFF", 4) || !_fmemcmp(head + 8, "AIFC", 4))) {
        s->container = RPF_AIFF; ok = open_aiff(s, head[11] == 'C');
    } else set_error(s, "Unknown file format.");
    if (ok) ok = seek_to(s, s->data_start);
    if (!ok) { if (!s->error[0]) set_error(s, "Cannot read the file."); _lclose(s->file); s->file = HFILE_ERROR; }
    return ok;
}

void RpClose(RPSTREAM FAR *s)
{
    if (s->file != HFILE_ERROR && s->file) _lclose(s->file);
    s->file = HFILE_ERROR;
}

static const short ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767 };
static const signed char ima_index[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

static short clamp16(long v) { return (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

static short ima_nibble(int nibble, long FAR *predictor, int FAR *index)
{
    int step = ima_steps[*index];
    long diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) *predictor -= diff; else *predictor += diff;
    *predictor = clamp16(*predictor);
    *index += ima_index[nibble];
    if (*index < 0) *index = 0;
    if (*index > 88) *index = 88;
    return (short)*predictor;
}

/* Decodes one IMA block (WAVE layout) into s->pending. */
static BOOL ima_block(RPSTREAM FAR *s, const BYTE FAR *b, WORD bytes)
{
    long predictor[2];
    int index[2];
    WORD ch, nch = s->channels, frame, i, k;
    const BYTE FAR *p;
    if (bytes < 4 * nch) return FALSE;
    for (ch = 0; ch < nch; ++ch) {
        predictor[ch] = (short)le16(b + 4 * ch);
        index[ch] = b[4 * ch + 2];
        if (index[ch] > 88) index[ch] = 88;
        s->pending[ch] = (short)predictor[ch];
    }
    p = b + 4 * nch;
    frame = 1;
    /* Data is interleaved in 4-byte (8-sample) groups per channel. */
    while (frame < s->block_frames && (WORD)(p - b) + 4 * nch <= bytes) {
        for (ch = 0; ch < nch; ++ch) {
            for (i = 0; i < 4; ++i) {
                BYTE v = p[4 * ch + i];
                for (k = 0; k < 2; ++k) {
                    WORD f = (WORD)(frame + i * 2 + k);
                    short sample = ima_nibble(k ? v >> 4 : v & 15, &predictor[ch], &index[ch]);
                    if (f < s->block_frames) s->pending[f * nch + ch] = sample;
                }
            }
        }
        p += 4 * nch;
        frame += 8;
    }
    s->pending_frames = frame < s->block_frames ? frame : s->block_frames;
    return TRUE;
}

static const int ms_adapt[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

static BOOL ms_block(RPSTREAM FAR *s, const BYTE FAR *b, WORD bytes)
{
    WORD nch = s->channels, ch, frame, nibble_index = 0;
    int predictor[2], delta[2];
    long s1[2], s2[2];
    const BYTE FAR *p;
    if (bytes < 7 * nch) return FALSE;
    for (ch = 0; ch < nch; ++ch) {
        predictor[ch] = b[ch];
        if (predictor[ch] >= (int)s->ms_ncoef) return FALSE;
        delta[ch] = (short)le16(b + nch + 2 * ch);
        s1[ch] = (short)le16(b + 3 * nch + 2 * ch);
        s2[ch] = (short)le16(b + 5 * nch + 2 * ch);
        s->pending[ch] = (short)s2[ch];
        s->pending[nch + ch] = (short)s1[ch];
    }
    p = b + 7 * nch;
    for (frame = 2; frame < s->block_frames; ++frame) {
        for (ch = 0; ch < nch; ++ch) {
            WORD byte_offset = (WORD)(nibble_index >> 1);
            int nibble, signed_nibble;
            long prediction;
            if ((WORD)(p - b) + byte_offset >= bytes) { s->pending_frames = frame; return TRUE; }
            nibble = (nibble_index & 1) ? p[byte_offset] & 15 : p[byte_offset] >> 4;
            ++nibble_index;
            signed_nibble = nibble >= 8 ? nibble - 16 : nibble;
            prediction = (s1[ch] * s->ms_coef1[predictor[ch]] + s2[ch] * s->ms_coef2[predictor[ch]]) / 256;
            prediction += (long)signed_nibble * delta[ch];
            s2[ch] = s1[ch];
            s1[ch] = clamp16(prediction);
            s->pending[frame * nch + ch] = (short)s1[ch];
            delta[ch] = (int)(((long)ms_adapt[nibble] * delta[ch]) / 256);
            if (delta[ch] < 16) delta[ch] = 16;
        }
    }
    s->pending_frames = s->block_frames;
    return TRUE;
}

static int decode_adpcm(RPSTREAM FAR *s, short FAR *out, WORD max_frames)
{
    WORD produced = 0, nch = s->channels;
    while (produced < max_frames) {
        WORD take;
        if (s->pending_used >= s->pending_frames) {
            HGLOBAL h;
            BYTE FAR *block;
            WORD bytes;
            BOOL ok;
            if (s->data_pos >= s->data_bytes) break;
            bytes = s->block_align;
            if ((DWORD)bytes > s->data_bytes - s->data_pos) bytes = (WORD)(s->data_bytes - s->data_pos);
            h = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (!h) { set_error(s, "Out of memory."); return -1; }
            block = (BYTE FAR *)GlobalLock(h);
            ok = read_exact(s, block, bytes);
            if (ok) ok = s->codec == RPC_IMA_ADPCM ? ima_block(s, block, bytes) : ms_block(s, block, bytes);
            GlobalUnlock(h); GlobalFree(h);
            s->data_pos += bytes;
            if (!ok) { set_error(s, "Damaged ADPCM block."); return -1; }
            s->pending_used = 0;
        }
        take = (WORD)(s->pending_frames - s->pending_used);
        if (take > max_frames - produced) take = (WORD)(max_frames - produced);
        _fmemcpy(out + produced * nch, s->pending + s->pending_used * nch, take * nch * sizeof(short));
        s->pending_used = (WORD)(s->pending_used + take);
        produced = (WORD)(produced + take);
    }
    s->frame_pos += produced;
    return (int)produced;
}

int RpDecode(RPSTREAM FAR *s, short FAR *out, WORD max_frames)
{
    BYTE raw[512];
    WORD produced = 0, nch = s->channels;
    if (s->codec == RPC_IMA_ADPCM || s->codec == RPC_MS_ADPCM) return decode_adpcm(s, out, max_frames);
    while (produced < max_frames) {
        WORD frames, bytes, i, n;
        if (s->data_pos >= s->data_bytes) {
            if (s->container == RPF_VOC && s->voc_more && voc_next(s, s->data_start + s->data_bytes)) continue;
            break;
        }
        frames = (WORD)(sizeof(raw) / s->block_align);
        if (frames > max_frames - produced) frames = (WORD)(max_frames - produced);
        bytes = (WORD)(frames * s->block_align);
        if ((DWORD)bytes > s->data_bytes - s->data_pos) {
            bytes = (WORD)(s->data_bytes - s->data_pos);
            frames = (WORD)(bytes / s->block_align);
            bytes = (WORD)(frames * s->block_align);
            if (!frames) { s->data_pos = s->data_bytes; continue; }
        }
        n = (WORD)_lread(s->file, raw, bytes);
        if (n == (WORD)HFILE_ERROR) { set_error(s, "Read error."); return -1; }
        frames = (WORD)(n / s->block_align);
        if (!frames) { s->data_pos = s->data_bytes; continue; }
        s->data_pos += (DWORD)frames * s->block_align;
        for (i = 0; i < frames * nch; ++i) {
            short v;
            switch (s->codec) {
            case RPC_PCM_U8: v = (short)(((int)raw[i] - 128) << 8); break;
            case RPC_PCM_S8: v = (short)((signed char)raw[i] << 8); break;
            case RPC_PCM_S16LE: v = (short)le16(raw + 2 * i); break;
            case RPC_PCM_S16BE: v = (short)be16(raw + 2 * i); break;
            case RPC_ULAW: v = RpUlaw(raw[i]); break;
            default: v = RpAlaw(raw[i]); break;
            }
            out[(produced * nch) + i] = v;
        }
        produced = (WORD)(produced + frames);
    }
    s->frame_pos += produced;
    return (int)produced;
}

BOOL RpSeek(RPSTREAM FAR *s, DWORD target)
{
    DWORD unit, blocks;
    if (s->container == RPF_VOC) {
        /* Multi-block VOC: restart and skip forward. */
        short scratch[256];
        BYTE head[26];
        if (!seek_to(s, 0) || !read_exact(s, head, 26)) return FALSE;
        s->rate = 0; s->voc_extended = FALSE;
        if (!voc_next(s, le16(head + 20))) return FALSE;
        s->frame_pos = 0;
        while (s->frame_pos + 128 <= target) if (RpDecode(s, scratch, 128) <= 0) break;
        return TRUE;
    }
    if (s->total_frames && target > s->total_frames) target = s->total_frames;
    unit = s->block_frames;
    blocks = target / unit;
    s->data_pos = blocks * s->block_align;
    if (s->data_pos > s->data_bytes) s->data_pos = s->data_bytes;
    s->frame_pos = blocks * unit;
    s->pending_frames = s->pending_used = 0;
    return seek_to(s, s->data_start + s->data_pos);
}

LPCSTR RpCodecName(const RPSTREAM FAR *s)
{
    switch (s->codec) {
    case RPC_PCM_U8: return "8-bit PCM";
    case RPC_PCM_S8: return "8-bit signed PCM";
    case RPC_PCM_S16LE: return "16-bit PCM";
    case RPC_PCM_S16BE: return "16-bit big-endian PCM";
    case RPC_ULAW: return "mu-law";
    case RPC_ALAW: return "A-law";
    case RPC_IMA_ADPCM: return "IMA ADPCM";
    case RPC_MS_ADPCM: return "Microsoft ADPCM";
    }
    return "unknown";
}
