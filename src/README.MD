# RetroPlayer

2026-11-04 (max-DX video output + Vista/NT6 DirectX + XP x64 / NT 3.51 tests):
the OS-aware video-output selection (PATCHES/VOUTSEL.DIF) now gives Windows
Vista and later the same Direct3D 9 -> DirectDraw -> software GDI chain as
Windows 2000/XP, instead of software GDI only. This is safe because the
Direct3D 11 output that crashed the i386/no-SIMD engine on modern Windows is
not shipped, and the Direct3D 9 output fails gracefully (falling through to
DirectDraw then GDI) when no Direct3D 9 device is present - as proven on the
test guests below, where the emulated Cirrus adapter has no 3-D and D3D9
cleanly declines. The current engine was booted and played on-screen on the
three supplied images: Windows 2000 (NT 5.0, AHCI) and Windows XP x64 (NT 5.2,
64-bit host, 32-bit player under WOW64) both logged the preference
"direct3d9,directdraw,rpgdi" and rendered H.264 through DirectDraw after D3D9
declined; Windows NT 3.51 logged "rpgdi" and rendered H.264 through the GDI
output. See RESULTS/XP64PLAY.* and RESULTS/N351HPLAY.* for the captures and
verbose logs. No push has been made.

2026-11-03 (VLC-style GUI, DirectX, installer, 98/2000): the control bar was
rebuilt in VLC's style - a draggable seek slider, elapsed/total time, a
Play/Pause toggle with Stop and 10-second seeks, a volume slider and mute, and
a Video menu to pick the output. Video output is now chosen by OS: DirectDraw /
Direct3D 9 on Windows 9x and NT 4/2000/XP (software GDI fallback), and software
GDI on NT 3.x and on Vista and later; the DirectX plugins ship again (253
plugins) and an RP_VOUT override backs the Video menu. The installer is in the
ZIP as SETUP\SETUP.EXE and installs to C:\RETROPLY on NT 3.x, to
%ProgramFiles%\Retro Player on 9x / NT 4+ (Program Files (x86) on x64). Booted
and played on Windows 98 (IDE) and Windows 2000 (AHCI) under QEMU, plus the
earlier Windows 95 and NT 3.1/3.51/4 coverage. See STATUS.TXT for details.

2026-11-03 (Windows 11 crash fix): a user reported RETRO.EXE crashing during
video playback on Windows 11. The engine's vout wrapper selects the display
module from the "vout" variable, which this build does not set (so the search
runs as "any"); on the retro guests the hardware outputs cannot initialise so
"any" always picked the legacy RPGDI software output, but on Windows 11
direct3d11 (priority 300) wins over RPGDI (100), and that Direct3D path - never
used on the i386/no-SIMD legacy targets - crashes. Fix: the hardware and
non-legacy video outputs (direct3d11/9, their filters, directdraw, wingdi) are
no longer shipped, leaving RPGDI as the only general vout display, so "any"
resolves to the software GDI output on every system including Windows 11
(250 plugins now, from 256; PLUGINS.DAT is rescanned on first run). Verified on
Windows 95 under QEMU; Windows 11 cannot be tested here, so please re-test it.

2026-11-03 (final GUI pass): the 32-bit player's user interface was finished -
a colour toolbar (Open / Play / Pause / Stop / Back / Forward / Go to start),
an amber-framed video area, a status bar and an application icon, all drawn
with core GDI so they render on NT 3.1 as well as Windows 95. The About box
and window title were corrected (the "development build / incomplete" wording
is gone), and the player now accepts a media path on the command line (for
file associations and drag-and-drop). A real 1280x720 H.264+AAC MP4 supplied
as a test was verified playing end to end through the rebuilt RETRO.EXE on
Windows 95 and on Windows NT 3.1 (RESULTS/W95USRV.PNG, RESULTS/N31USRV.PNG).
The new binary is integrated into BIN, DIST, BUILD/ASM and the regenerated
installer payload (BUILD/SETUP).

Both players are complete: the 32-bit RETRO.EXE runs the VLC 3.0.24 engine
with full VLC/FFmpeg decoder and demuxer coverage (FORMATS.MD), and the native
16-bit RETRO16.EXE plays on Windows 3.0/3.1 (WIN16.MD, SRC16). Win32s was
tested and cannot host LibVLC (no threads). See the top of STATUS.TXT.

The patched VLC 3.0.24 core initializes on NT 3.1, NT 3.51, NT 4 and
Windows 95. A generated PCM WAV decodes to 16000 byte-exact audio samples on
each guest. H.264, HEVC, VP9, AV1 and AAC software-decode tests pass on all
four guests with two codec workers. The legacy GDI output renders AV1 in all
four frontends. The assembly profile also decodes PNG/ZMBV, renders SRT and
ASS Unicode subtitles, uses a working plugin cache, and passes pause/seek/
replay tests on all four guests. Very high-resolution modern clips (for
example 720p30) decode correctly but cannot be displayed in real time on
period-class CPUs under emulation; that is a hardware limit, not a format gap.
The additional audio matrix covers 24 cases, including Ogg/Matroska Opus,
MP3, FLAC, ALAC, Vorbis, AC-3/E-AC-3, WMA, WavPack, short streams and chained
Opus/Vorbis on all four guests. Another 16 video/container cases cover older
codecs, MPEG transport/program streams, AVI, ASF, Ogg, Matroska and QuickTime;
VFORMAT.MD records the exact pixel comparison limits.
No push has been made. STATUS.MD distinguishes each validation scope.

HDA output is configured with the requested WINAUDIO (Windows 95) and
AUDIONT (NT3.1/3.51/4) drivers. All four guests passed actual emulated-device
tone-capture checks. HDA.MD describes installation, startup clock correction
and the test limits. DRIVERS contains the pinned drivers with licenses;
SOURCES includes their source archives. Sound Blaster has a separate optional comparison profile.
An earlier Win95 SB16 comparison passed both waveOut and independent captured
tone checks. The changed core needs a fresh SB16 comparison before packaging.
The NT audio results use HDA. Those earlier passes do not certify the current candidates: NT3.51 JM/JN
fails long audio and A/V capture checks, and Win95 JM retains A/V failures.
STATUS.MD and AVSYNC.MD record the current evidence.

Read TASK.MD for the current scope and acceptance targets. Complete 32-bit
support takes priority over 16-bit implementation.

## Contents

- SRC/RETRO.C: CRT-free Win32 frontend with LibVLC 3.x dynamic bindings,
  open/play/pause/stop and ten-second seek menus, native NT Unicode file dialogs,
  and explicit missing-engine handling. Actual dialog, GDI and menu tests
  pass on NT3.1/NT3.51/NT4/Win95; STATUS.MD records the precise scope.
- SRC/PROBE.C: guest DLL/API availability and real Unicode file-read probe.
  Export presence alone does not establish correct behavior or acceleration.
- DIST/RETRO.EXE and DIST/PROBE.EXE: 32-bit development executables.
- SCRIPTS/AUDIT.PY: DOS names, name collisions, PE architecture and subsystem
  checks. This does not replace OS, processor or playback tests.
- RESULTS: guest reports, screenshots and import tables.
- HDA.MD and AFORMAT.MD: requested audio drivers and decoded-audio verification.
- SRC/ATOM.C, KERN.C, UTF8.C, CRT.C, CRTFILE.C, ALIGN.C, ENV.C, SYNC.C, TLS.C and NET.C:
  legacy runtime adapters; their independent tests do not prove playback.
- PATCHES/LEGACY.DIF, MINGW.DIF, FFMPEG.DIF, DAV1D.DIF and GCC.DIF: source changes against verified bases.
- BUILD/MEDIA: the tested single-worker modern-codec diagnostic package.
- BUILD/SMP: the tested two-worker/AV1 diagnostic profile.
- BUILD/ASM: experimental assembly, compressed media, text and cache profile.
- BUILD/SETUP: development installer with verified payloads and rollback;
  INSTALL.MD records its validation status and limitations.
- BUILD/ENGINE: private core/PCM diagnostic package, not a full player release.
- TESTDISK: user images and disposable working copies, excluded from packages.

RetroPlayer source and distribution filenames use DOS 8.3 naming. The
external reference source trees and Linux build tools retain upstream names.
Distributed diagnostic plugins have stable hashed 8.3 DLL names, mapped in
PLUGINS/UPSTREAM.TXT. The legacy loader accepts these names. Final packaging
and cache/path validation remain necessary for the full module set.

## Build the frontend and probe

The current host is Debian 13 x86-64. Tools include GCC 14.2.0 MinGW-w64 and
QEMU 10.0.13, extracted under /workspace/TOOLS/SYSROOT without a system install.
From /workspace/RETRO:

    sh SETUP.SH
    sh FETCH.SH

To rebuild using the installed tools:

    sh BUILD.SH
    python3 TESTS/AUDIT.PY

The compile flags include -march=i386 -mno-sse -mno-sse2 -mno-mmx. SDK headers
need WINVER=0x0400 to parse; the PE subsystem version is 3.10. The executables
do not link to the default MinGW CRT. Actual 80386 hardware is untested;
the guest tests use QEMU's Pentium model.

BUILDENV.SH activates the additional tools for engine development. SETUP.SH
also builds the independent runtime tests. For the diagnostic VLC core:

    sh PREP.SH
    sh MWCRT.SH
    sh CORE.SH
    sh COREST.SH

MWCRT.SH rebuilds the MinGW C support objects for i386 and removes native PE
TLS, which the Windows 95 loader rejects. RPCRT provides GCC emulated TLS;
MinGW's DLL startup still dispatches its constructor/destructor callbacks.
The frontend remains CRT-free. FFCORE.SH is a temporary software-codec
bring-up profile. RP_FF_THREADS=1 enables the ported Win32 worker threads;
assembly and most external libraries are still disabled. This is not the
requested final VLC feature set. The separate AV1 decoder uses dav1d 1.5.4.

The current worker-thread and AV1 profile can be built with:

    RP_FF_DIR=/workspace/FFTHR RP_FF_PREFIX=/workspace/THRLIB RP_FF_THREADS=1 sh FFCORE.SH
    sh DAVCORE.SH
    RP_CORE_DIR=/workspace/THRBLD RP_CORE_MEDIA=1 RP_CORE_AV1=1 RP_CODEC_PREFIX=/workspace/THRLIB sh CORE.SH
    RP_CORE_DIR=/workspace/THRBLD RP_CORE_MEDIA=1 RP_CORE_AV1=1 RP_CORE_THREADS=2 RP_CORE_OUT=BUILD/SMP sh COREST.SH

Build the compatibility runtimes before FFCORE.SH and DAVCORE.SH. The package
contains MEDTRY.EXE, which runs fresh kernel, synchronization, CRT, engine,
PCM and modern-media checks. CPU threads and physical outputs remain separate
validation scopes. Host-generated fixtures and independent frame hashes come
from SCRIPTS/SAMPLES.PY; ffmpeg must be available on the build host.

## Guest tests

Each cloud task is already isolated. Do not create a Git worktree unless the
user explicitly requests one. Preserve original images. VM.SH opens writable
QCOW2 working copies with no network interface:

    sh VM.SH NT31
    sh VM.SH W95

Screenshots are available through local QMP sockets, for example:

    python3 SCRIPTS/QMP.PY /tmp/NT31.SOC screendump '{"filename":"/workspace/RETRO/RESULTS/NT31.PPM"}'

NT 3.1: after Ctrl+Alt+Delete, the existing test account logged in with a blank
password. Program Manager > File > Run: A:\RETRO.EXE /smoke or A:\PROBE.EXE.
The floppy image is TESTDISK/NT31.FLP. The smoke run creates REPORT.TXT and
exits; PROBE.EXE creates CAPS.TXT. Omit /smoke for normal GUI startup.

Windows 95: this installation did not expose A:. The powered-off working
image was converted to RAW and the executables copied to
C:\Program Files\RetroPlayer. VM.SH prefers W95EDIT.QC2 when present.
Reports are stored beside the executables. NT3.xx uses C:\RETROPLA and
NT4 uses the same Program Files path as Windows 95.
After a clean guest shutdown, qemu-img convert can produce a RAW inspection
copy; the Windows 95 FAT16 partition offset is 32256 bytes. Do not modify a
mounted/running disk with mtools, and do not assume the same partition offset
for other images.

Final installation paths are C:\RETROPLA for Win3.x/NT3.xx and
C:\Program Files\RetroPlayer for NT4/Windows 95/98/ME. The development installer has been tested on all four Win32 guests;
its payload predates the current candidates and needs rebuilding after acceptance. Long parent directories do not relax the 8.3 constraint
on application filenames.

Guest processes do not survive a new cloud task; restart them as needed.
Publication and fresh-task restoration have not been verified.

## VLC base

Official mirror: https://github.com/videolan/vlc

- Tag: 3.0.24, the newest stable tag observed during this work.
- Commit: 6de05adcbaf2e8b85fe86aad4169393098628119.
- Reference: /workspace/VLCREF; engine work: /workspace/VLCWORK.
- The upstream NEWS and contrib rules specify FFmpeg 8.1.2.

The CRT, synchronization, filesystem, Unicode, plugins and output modules
require an actual legacy port. NT 3.1 lacks CreateDIBSection used by VLC's GDI
module. Long names such as LIBVLCCORE.DLL cannot be fixed by merely renaming
a file: import tables, build rules, lookup paths and caches must agree.
Codec scope has not been reduced to a small list of example formats, but no
claim of complete codec integration is made. Acceleration requires working OS APIs, drivers
and hardware. STATUS.MD distinguishes tested behavior from unfinished work.

## C++ runtime development

CXXPREP.SH verifies and prepares GCC 14.2.0 source. CXXCORE.SH rebuilds the
C++ library and i386 exception unwinder; CXXTEST.SH builds the private DLL
regression suite. The shared gthread adapter lives in RPCRT. CXXSUITE.EXE
runs atomics, kernel APIs, condition variables, CRT and C++ checks with child
timeouts. This suite is independent of the media playback package.

## Experimental compression and text profile

After the runtime, C++ and dav1d prerequisites are built:

    sh DEPCORE.SH
    RP_FF_DIR=/workspace/FFTHR RP_FF_PREFIX=/workspace/THRLIB RP_FF_THREADS=1 sh FFCORE.SH
    RP_CORE_DIR=/workspace/TXTBLD RP_CORE_MEDIA=1 RP_CORE_AV1=1 RP_CORE_ALL=1 RP_CORE_TEXT=1 RP_CODEC_PREFIX=/workspace/THRLIB sh CORE.SH
    sh DEPTST.SH
    sh GUARD.SH

DEPS.JSN and FONT.SHA in SOURCES pin the dependency archives and bundled
DejaVu Sans font. Their licenses are in LICENSES. FreeType and libxml2
patches are FREETYP.DIF and LIBXML.DIF. RPLOAD.H performs a bounded PE import
preflight before attempting optional plugin loads. GUARD.EXE additionally
exercises malformed input and shared dependency lifetime during unloading.
BUILD/SMP remains an independent playback checkpoint. PACK.PY includes both
that profile and BUILD/ASM; neither is a complete release. See STATUS.MD for
individual regression results and incomplete coverage.

## Assembly and text integration

ICONV.SH builds GNU libiconv's encoding tables for i386/CRTDLL. DEPCORE.SH
includes it. To prepare the experimental assembly prefix after DEPCORE.SH:

    mkdir -p /workspace/ASMLIB
    cp -a /workspace/THRLIB/include /workspace/THRLIB/lib /workspace/ASMLIB/
    RP_FF_DIR=/workspace/FFASM RP_FF_PREFIX=/workspace/ASMLIB RP_FF_THREADS=1 RP_FF_ASM=1 sh FFCORE.SH
    RP_CORE_DIR=/workspace/ASMBLD RP_CORE_MEDIA=1 RP_CORE_AV1=1 RP_CORE_ALL=1 RP_CORE_TEXT=1 RP_CODEC_PREFIX=/workspace/ASMLIB sh CORE.SH
    sh ASMPACK.SH

The FFmpeg dispatch adapter permits CPU-supported MMX while rejecting XMM-based
code when the operating system cannot preserve XMM state. This is distinct
from GPU/video acceleration. CPUTEST reports both CPU and OS capabilities.
TXTTRY.EXE runs the combined file, cache, runtime, dependency and media tests.
The frontend generates a missing plugin cache and keeps directory scanning
enabled so stale or absent cache entries do not hide installed plugins.

VM.SH creates a private COM1 Unix socket. After launching VMAGENT.EXE in the
guest, wait for PING to return PONG, then run allowlisted tests and fetch reports:

    python3 SCRIPTS/VMRUN.PY NT31 PING
    python3 SCRIPTS/VMRUN.PY NT31 RUN TXTTRY.EXE
    python3 SCRIPTS/VMRUN.PY NT31 STAT
    python3 SCRIPTS/VMRUN.PY NT31 GET SUITE.TXT --output RESULTS/N31CHECK.TXT

GET verifies the transferred byte count and checksum. A report read during a
running test is only a progress snapshot. Wait for a completed process and the
final RESULT line before interpreting it as a full test result. VMIO.PY transfers
application files only after the disposable guest has shut down and QEMU exited.


The experimental profile also includes SEEKTEST.EXE (independent frame-identity
checks through pause, seek and replay) and UITEST.EXE (real dialogs, Unicode
media names and GDI output). Run UITEST separately on an unobstructed guest
desktop after TXTTRY.EXE finishes. It temporarily creates and removes its own
media copy. Source fixtures are regenerated with SEEKTEST.SH; UITEST.SH builds
the GUI test. These tests do not validate physical A/V synchronization.
