/* RetroPlayer 16 - container parsing and software audio decoding.
 * Supported: RIFF WAVE (PCM 8/16, IMA/DVI ADPCM, Microsoft ADPCM, mu-law,
 * A-law, WAVE_FORMAT_EXTENSIBLE PCM), Creative VOC (8-bit, 16-bit, mu-law,
 * A-law), Sun/NeXT AU (8/16-bit linear, mu-law, A-law) and AIFF/AIFC
 * ("NONE"/"sowt" PCM). Output is interleaved signed 16-bit PCM. */
#ifndef RP16_DECODE_H
#define RP16_DECODE_H

#include <windows.h>

#define RPF_UNKNOWN 0
#define RPF_WAVE    1
#define RPF_VOC     2
#define RPF_AU      3
#define RPF_AIFF    4

#define RPC_PCM_U8     1   /* unsigned 8-bit */
#define RPC_PCM_S8     2   /* signed 8-bit */
#define RPC_PCM_S16LE  3
#define RPC_PCM_S16BE  4
#define RPC_ULAW       5
#define RPC_ALAW       6
#define RPC_IMA_ADPCM  7
#define RPC_MS_ADPCM   8

#define RP_MAX_CHANNELS 2

typedef struct tagRPSTREAM {
    HFILE file;
    int container;
    int codec;
    WORD channels;
    DWORD rate;
    WORD bits;            /* bits per coded sample */
    WORD block_align;     /* bytes per coded block (ADPCM) or frame (PCM) */
    WORD block_frames;    /* frames per block for ADPCM, 1 for PCM */
    DWORD data_start;     /* file offset of the first coded byte */
    DWORD data_bytes;     /* coded bytes in the current data run */
    DWORD data_pos;       /* coded bytes consumed in the current data run */
    DWORD total_frames;   /* 0 when unknown */
    DWORD frame_pos;      /* decoded frames delivered so far */
    /* VOC: blocks may follow each other */
    BOOL voc_more;
    BOOL voc_extended;   /* a type 8 block set rate/channels */
    /* MS ADPCM coefficients */
    WORD ms_ncoef;
    short ms_coef1[32];
    short ms_coef2[32];
    /* pending decoded frames from a partially returned ADPCM block */
    short pending[2 * 2041];
    WORD pending_frames;
    WORD pending_used;
    char error[64];
} RPSTREAM;

/* Opens and identifies the file. Returns FALSE with stream->error set. */
BOOL RpOpen(RPSTREAM FAR *s, LPCSTR path);
void RpClose(RPSTREAM FAR *s);
/* Decodes up to max_frames frames into out (channels interleaved).
 * Returns frames produced; 0 at end of stream, -1 on error. */
int RpDecode(RPSTREAM FAR *s, short FAR *out, WORD max_frames);
/* Seeks to the frame nearest at or before target. */
BOOL RpSeek(RPSTREAM FAR *s, DWORD target_frame);
/* Human readable codec name for the status line. */
LPCSTR RpCodecName(const RPSTREAM FAR *s);

/* Pure helpers, exposed for the host regression test. */
short RpUlaw(BYTE v);
short RpAlaw(BYTE v);
DWORD RpExtendedRate(const BYTE FAR *ext80);

#endif
