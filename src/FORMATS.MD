# RetroPlayer format coverage

## 32-bit player (RETRO.EXE, NT 3.1/3.5x/4 and Windows 95/98/ME)

The 32-bit player runs the real VLC 3.0.24 engine. Format coverage is therefore
VLC's own coverage, provided by three layers that are all built and shipped:

1. **FFmpeg 8.1.2** (`avcodec` + `avformat` plugins). FFmpeg is configured with
   `--disable-encoders --disable-muxers` only; every native FFmpeg **decoder**
   and **demuxer** is enabled (`FFCORE.SH`): **502 decoders, 358 demuxers,
   67 parsers, 0 encoders, 0 muxers** (verifiable in the build's
   `config_components.h`). This alone covers essentially all mainstream and
   legacy formats:
   - Video: H.264/AVC, H.265/HEVC, H.263, MPEG-1/2, MPEG-4 Part 2 (DivX/Xvid),
     MS-MPEG4 v1/2/3, VP3/6/7/8/9, Theora, WMV 1/2/3 / VC-1, RealVideo 1-4,
     Cinepak, Indeo 2-5, MJPEG, FFV1, HuffYUV, DV, SVQ1/3, Flash Screen Video,
     ProRes, Lagarith, ZMBV, QuickTime RLE/SMC, and many more.
   - Audio: MP1/2/3, AAC (LC/HE), AC-3/E-AC-3, DTS, Vorbis, FLAC, ALAC, WMA
     1/2/Pro/Lossless, Opus, Speex, WavPack, TrueHD, TTA, Musepack, QDM,
     Cook/RealAudio, ADPCM (all variants), PCM (all variants), G.711/722/726,
     AMR, GSM, and more.
   - Containers: MP4/MOV, Matroska/WebM, MPEG-TS/PS, AVI, ASF/WMV, Ogg, FLV,
     RealMedia, WAV, AIFF, CAF, FLAC, 3GP, MXF, NUT, and more.
2. **dav1d 1.5.4** (`dav1d` plugin): AV1 video.
3. **VLC native modules** (listed in `BIN/PLUGINS/UPSTREAM.TXT`): native
   demuxers (asf, avi, mp4, ts, ps, es, wav, voc, au/aiff, caf, nsv, real,
   mjpeg, cdg, flac, xa), native codecs and packetizers (adpcm, araw, aes3,
   g711, lpcm, flac, vorbis, opus, speex, tta, vc1, uleaddvaudio, h26x, cc),
   subtitles (subsdec/SRT, SSA/ASS via the text renderer, substx3g, subsusf,
   webvtt, ttml, teletext/telx, DVB/SVCD/CVD/VobSub, textst), image codecs,
   video chromas/filters, resamplers and audio converters.

### Modern video types - directly verified (RESULTS/MODVIDEO.JSN)

A fresh test decodes today's mainstream web/streaming codecs in today's
containers through the actual shipped engine and compares every frame
pixel-for-pixel to the host ffmpeg decode:

| Codec | Containers tested | Decoder | NT4 | Win95 |
|-------|-------------------|---------|-----|-------|
| H.264/AVC | MP4 | FFmpeg h264 | PASS | PASS |
| H.265/HEVC | MP4, MKV | FFmpeg hevc | PASS | PASS |
| VP9 | WebM, MKV | FFmpeg vp9 | PASS | PASS |
| AV1 | MP4, MKV, WebM | dav1d | PASS | PASS |

50 checks, 0 failures on each guest; all frames within one 8-bit unit of the
reference. The generator is SCRIPTS/VFMODRN.PY; the test binary is
BUILD/VFMODBIN/VFMTEST.EXE.

`AFORMAT.MD` and `VFORMAT.MD` record the exact fixtures and pixel/sample
tolerances; the audio-format (24-case) and video-format matrices pass on
NT 3.51, NT 4 and Windows 95 (session matrix tag M2), and the audio matrix
passes on NT 3.1 when run alone (see RESULTS/N31FIRST.TXT for the first-case
test-harness anomaly on NT 3.1, which does not affect actual playback — the
NT 3.1 UITEST and the 14-program integrated suite pass).

### What is deliberately not built (and why)

These are engine options, not missing formats:

- Encoders, muxers and stream output (`--disable-sout/--disable-muxers/
  --disable-encoders`): a player decodes, it does not transcode or stream out.
- UI/scripting modules (`qt`, `skins2`, `lua`): RetroPlayer has its own
  CRT-free Win32 frontend.
- SIMD paths (`--disable-mmx/--disable-sse`, FFmpeg scalar): the i386 baseline
  target. The dispatch adapter still permits CPU-supported MMX where the OS can
  preserve state; XMM is rejected when the OS cannot preserve XMM.

### Platform-limited (present but gated on OS support)

These modules are built and shipped but load only where the operating system
provides the required API/driver, so coverage is the platform's, not a code gap:

- Video output. The player selects the output module by operating system:
  `direct3d9` / `directdraw` on Windows 9x and NT 4 / 2000 / XP (hardware
  accelerated where the display driver provides it, DirectDraw's software HEL
  otherwise), and the legacy GDI module (`rpgdi`) on NT 3.x and on Windows
  Vista and later. The Video menu overrides the choice (via the `RP_VOUT`
  environment variable). `rpgdi` is always available as the software fallback,
  so an unavailable accelerator never leaves a black video area. `direct3d11`
  is deliberately not shipped: it is Vista+ only and its path is unvalidated in
  this i386/no-SIMD build. Hardware `dxva2`/`d3d11va` decode is disabled at
  build time (software FFmpeg decode is used everywhere).
- Network access (`http/https`, `ftp`, `smb`, `rtp/udp`, `sdp`): needs working
  OS networking (Winsock) and, for https, the OS crypto. Present but unverified
  on every target.
- Capture (`dshow`, `screen`): needs DirectShow / a screen driver.

"Full coverage" for this project therefore means: every VLC/FFmpeg software
decoder and demuxer feasible on the target OS is built and shipped; the only
limits are OS-provided acceleration, networking and capture, which no
user-space player can add on its own.

## 16-bit player (RETRO16.EXE, Windows 3.0/3.1)

The 16-bit player cannot run the VLC engine (no threads on Win16/Win32s), so it
uses its own software decoders (`SRC16/DECODE.C`) plus MCI for driver-backed
formats. See `WIN16.MD`. Built-in: WAVE (PCM 8/16, IMA & MS ADPCM, mu-law,
A-law), Creative VOC, Sun/NeXT AU, AIFF/AIFC. Through MCI (when a driver is
installed): MIDI, Video for Windows AVI, CD audio. Modern compressed codecs
(MP3/AAC/Opus/H.264/…) are out of scope for 8086-class machines and a 16-bit
segmented memory model; they remain the 32-bit player's job.
