# RetroPlayer 16 (native Windows 3.0 / 3.1 player)

RETRO16.EXE is a separate 16-bit NE program for Windows 3.0 (real, standard
and 386 enhanced mode) and Windows 3.1 (standard and 386 enhanced mode).
It is not a port of LibVLC: the VLC 3.x engine needs threads, kernel event
objects and a flat 32-bit address space. Win32s cannot provide those either
(RESULTS/W32STEST.TXT: CreateThread and CreateEvent return error 120), so a
native program is the only way to play media on Windows 3.x.

## Scope

Built-in software decoding (SRC16/DECODE.C), no external codecs required:

| Container | Codecs |
|-----------|--------|
| RIFF WAVE | PCM 8/16-bit, IMA (DVI) ADPCM, Microsoft ADPCM, mu-law, A-law, WAVE_FORMAT_EXTENSIBLE PCM |
| Creative VOC | 8-bit (block types 1, 2, 8), 16-bit, mu-law, A-law (type 9) |
| Sun/NeXT AU (.AU/.SND) | 8-bit signed, 16-bit, mu-law, A-law |
| AIFF / AIFC | 8/16-bit big-endian, "sowt" little-endian, ulaw, alaw |

Mono and stereo, 1-96 kHz. Output is resampled (linear interpolation) and
converted to whatever the device accepts.

Through MCI when MMSYSTEM.DLL and the matching driver are installed
(Windows 3.1, or 3.0 with Multimedia Extensions): MIDI (MCISEQ), Video for
Windows AVI (MCIAVI with Indeo, Cinepak, Video 1 and RLE on the supplied 3.1
installation), CD audio and any other installed MCI device. Without MMSYSTEM
those files are reported as unsupported rather than failing silently.

Not in scope for the 16-bit program: MP3, AAC, Opus, Vorbis, FLAC, H.264,
HEVC, VP9, AV1 and other modern codecs. Real-time software decoding of these
is not feasible on the 8086-286 class machines that Windows 3.0 real and
standard mode target, and their reference decoders assume a 32-bit flat
memory model. They remain the 32-bit player's job (NT 3.1+ and 9x).

## Audio output

1. MMSYSTEM waveOut with four 8 KiB buffers and window callbacks. MMSYSTEM
   and COMMDLG are loaded dynamically, so the executable also starts on a
   plain Windows 3.0 installation.
2. When there is no MMSYSTEM or no wave device: direct Sound Blaster
   programming. The BLASTER variable selects the port and 8-bit DMA channel
   (defaults A220 D1). DSP 2.0+ is required; DSP 4.x (SB16) plays 8-bit stereo
   up to 44.1 kHz, older DSPs 8-bit mono up to 22.05 kHz. The DMA buffer comes
   from GlobalDosAlloc (below 1 MiB) and never crosses a 64 KiB page. No
   interrupt handler is installed; refill is driven from a Windows timer.
   - **Real and standard mode** (no paging, no DMA virtualisation): auto-init
     (continuous) DMA over a double buffer; the timer only refills the half the
     controller has passed. Gap-free.
   - **386 enhanced mode**: Windows 3.0 VDMAD neither virtualises auto-init DMA
     (programming the controller's mode register with the auto-init bit faults)
     nor keeps the virtual DMA counter current. So the player instead
     Virtual-DMA-Services-locks the buffer (INT 4Bh / VDS 8103h) to pin its
     pages and obtain a stable mapping, programs single-cycle DMA one half at a
     time, and paces block changeover by elapsed time (GetTickCount) rather than
     by reading the (stale) controller count. Verified gap-free in practice
     against the real-mode capture.
   If a real wave driver is installed, the player uses MMSYSTEM waveOut (above)
   and none of this applies.

## Interface

File (Open, Exit), Playback (Play, Pause/Resume, Stop, Back/Forward 10 s,
Go to beginning) and Help/About, matching the 32-bit player. The common Open
dialog is used when COMMDLG.DLL exists (3.1); on 3.0 an equivalent built-in
dialog (DlgDirList) is used. The About box reports the Windows version, the
running mode and the output path.

`RETRO16.EXE /test FILE [REPORT]` plays FILE unattended and writes a report
(default C:\RETROPLA\REPORT16.TXT) for guest automation.

## Memory model

Large model, 8086 instructions only (`-0`), movable code and data. Decoder
state is one movable global block that is locked only while a message is
processed, so real-mode memory compaction stays possible.

## Build and test

`sh BUILD16.SH` builds DIST16/RETRO16.EXE with Open Watcom v2 (wcc, wrc,
wlink). `TESTS16/HOSTDEC.C` builds DECODE.C natively on the host with a
small Win16 API shim and compares the decoded PCM with ffmpeg:

    gcc -x c -std=c99 -ITESTS16/HOST -ISRC16 -o HOSTDEC TESTS16/HOSTDEC.C SRC16/DECODE.C

Results are in RESULTS/W16DEC.JSN. Fixtures for the guests are in
BUILD/W16MEDIA (host regression inputs in BUILD/W16MEDIA/HOST) and are copied to C:\RETROPLA on the Win3.0/3.1 test disks,
whose AUTOEXEC.BAT also sets `BLASTER=A220 I5 D1 H5 T6` for QEMU's sb16.


## Guest test results (RESULTS/W16GUEST.JSN)

RETRO16.EXE was built with Open Watcom v2 as a 16-bit NE executable (imports
only KERNEL/USER/GDI; MMSYSTEM and COMMDLG resolved at run time) and tested on
the supplied Windows 3.0 and 3.1 images under QEMU with a Sound Blaster 16
(0x220/IRQ5/DMA1). Neither image has a wave driver, so the direct Sound Blaster
backend was exercised in every case. Audio was captured to host WAV and the
441/735/315 Hz test tones were confirmed exact with an FFT.

| Guest | Mode | Input | Result |
|-------|------|-------|--------|
| Windows 3.0 | real | 16-bit PCM WAV | PASS, gap-free (auto-init DMA) |
| Windows 3.0 | standard | 16-bit PCM WAV | PASS, gap-free |
| Windows 3.0 | 386 enhanced | 16-bit PCM WAV | PASS (VDS + single-cycle), no gaps vs real |
| Windows 3.1 | standard | Creative VOC | PASS |
| Windows 3.1 | 386 enhanced | IMA ADPCM WAV | PASS (decoded then played) |

The interactive GUI was also verified on Windows 3.0 (386 enhanced): the
built-in Open dialog lists files and directories and navigates drives (COMMDLG
is absent on 3.0), playback runs with an advancing position counter, Help/About
reports the correct Windows version, mode and output path, and the custom icon
renders. Screenshots are in RESULTS (W30UI*, W30REAL2, W30ENH2, W31ENH2,
W31STD1). The decoder itself is additionally checked bit-exact against ffmpeg on
the host (RESULTS/W16DEC.JSN).
