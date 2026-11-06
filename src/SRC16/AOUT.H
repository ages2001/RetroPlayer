/* RetroPlayer 16 - audio output backends.
 * 1. MMSYSTEM waveOut (Windows 3.1, or 3.0 with Multimedia Extensions),
 *    loaded dynamically so the program still starts without MMSYSTEM.DLL.
 * 2. Direct Sound Blaster DSP + 8237 DMA (Windows 3.0 real, standard and
 *    enhanced mode without a wave driver). Uses the BLASTER variable. */
#ifndef RP16_AOUT_H
#define RP16_AOUT_H

#include <windows.h>

#define AO_NONE     0
#define AO_WAVEOUT  1
#define AO_BLASTER  2

/* Fills up to max_frames frames of output-format PCM into buffer.
 * Returns frames written; 0 means end of stream. */
typedef WORD (FAR *AO_FILL)(void FAR *context, BYTE FAR *buffer, WORD max_frames);

typedef struct tagAOFORMAT {
    DWORD rate;
    WORD channels;
    WORD bits;          /* 8 (unsigned) or 16 (signed) */
} AOFORMAT;

typedef struct tagAOSTATUS {
    int backend;
    BOOL playing;
    BOOL paused;
    BOOL finished;
    DWORD frames_played;
    char device[64];
} AOSTATUS;

/* Chooses a backend and an output format no richer than the source.
 * The format may be reduced (channels/bits/rate) to what the device accepts. */
BOOL AoOpen(HWND notify, const AOFORMAT FAR *wanted, AOFORMAT FAR *actual,
            AO_FILL fill, void FAR *context, LPSTR error);
BOOL AoStart(void);
void AoPause(BOOL pause);
void AoStop(void);           /* stops and releases the device */
void AoReset(void);          /* drops queued audio; keeps the device open */
/* Must be called from WM_TIMER (Blaster polling) and on MM_WOM_DONE. */
void AoService(void);
void AoGetStatus(AOSTATUS FAR *status);
BOOL AoHasMmsystem(void);

#define AO_TIMER_ID 0x52

#endif
