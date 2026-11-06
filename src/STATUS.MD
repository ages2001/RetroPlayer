# Validation status — handoff 2026-11-02; guest matrix 2026-10-26

## Overall status

The complete RetroPlayer port is unfinished. The Win32 frontend and test
infrastructure work on the supplied NT 3.1 and Windows 95 installations.
The engine, codec dependencies, acceleration and 16-bit implementation remain
engineering work, not a setting that can be enabled by approving a draft.
See TASK.MD for the latest user requirements, including NT4/NT3.51 tests.

## Latest verified checkpoint

Current MMTSC2 candidate: all twelve idle native clock calibrations select
the processor clock across NT3.1, NT3.51, NT4 and Win95. This is diagnostic
evidence, not playback acceptance. K5's seven content jobs pass on NT3.51
and Win95. NT3.1 passes six jobs; its first MP3 case reaches the unchanged
20-second limit, while the other 23 audio cases pass. NT4 also passes all seven K5 jobs. K5PROOF.JSN retains the failed NT3.1
case. The unchanged NT3.1 audio-format test passes all 24 cases when run
alone (N31KAPF.JSN); the earlier timeout remains recorded. The isolated
K7/K9 device-output matrix passes 15 of 16 checks (K7PROOF.JSN): NT3.1,
NT4 and Win95 pass all four; NT3.51 passes the audio-only checks but fails
one HEVC scene by 338–406 ms. The candidate remains
outside BUILD/ASM and the installer. The supplied Windows 3.1 VDI and three
Win32s setup disks are prepared for the requested compatibility check.
Windows 3.1 has booted in enhanced mode. The first Win32s setup attempt
failed on source layout and cancellation reported a missing OLE backup.
W32RETRY.JSN records a clean prepared retry with SHARE and disk directories;
it has not been booted. FreeCell and RetroPlayer on Win32s remain untested.
BIN is a handoff copy of the current candidate, with source hashes recorded
in BINSTATE.JSN. This does not promote it into ASM or the older installer.
STATUS.TXT records the complete requested scope and outstanding work.

BUILD/ASM contains the priority-gate, resampler arithmetic, startup-clock and
queued waveOut fixes described in AVSYNC.MD. NT3.1 Y2 passes all eleven guest
programs. The closed N31ABSN.WAV passes N31ABCK.JSN (80 A/V scenes),
N31Y2QD.JSN (5–200 ms clips) and N31Y2HD.JSN (continuous tones).
Y2READY.JSN verifies 380 package/test files on each of the four stopped disks.
It follows correction of four stale NT3.1 test EXEs and older NT4/Win95 plugins;
the affected NT3.1 integrated suite passes in N31Y3SU.TXT; the newer NT3.1
UITEST passes separately in N31Y4UI.TXT. NT3.51 Y2 passes all twelve guest
programs. Its closed N351ABSN.WAV also passes the 80-scene A/V, short-clip
and continuous-tone checks (N351ABCK, N351Y2QD and N351Y2HD.JSN).
NT4 Y2 passes all twelve guest programs, short clips, continuous tones and
all 289 installed payload hashes. Its A/V capture N40ABCK.JSN fails two
checks. A first-picture synchronization candidate regressed NT4 output and
has been rejected. Win95 Y2 passes all twelve guest programs, short clips,
continuous tones and all 289 installed payload hashes, but its independent
A/V check fails. Bounded/coalesced waveOut candidates remove driver queue
errors; the measured-latency waveOut candidate now passes isolated Opus packet-size,
short-clip and tone captures on NT3.1 and Win95. Complete four-guest output
acceptance is still pending. These candidates are not in ASM.
Native Speex and its resampler pass S4 on all four guests: ten exact-length
PCM cases, 1961 current resampler checks and 1956 unchanged-upstream checks
per guest. Existing audio/video regressions also pass. SPINTEG.JSN records
integration of the three tested Ogg/Speex DLLs into ASM (256 modules);
the installer still contains the preceding 254-module payload. The first
dynamic waveOut latency candidate passes NT4's independent AV2, Opus packet,
short-clip and tone captures; its improved candidate still needs the complete
four-guest output matrix (AVSYNC.MD). The condition-event/cold-clock core
passes 308 dedicated checks per guest but exposed intermittent first-frame
loss in content regressions. Queue traces prove a pre-buffering forced
preview can be flushed by the rebased clock before display. COREPRV fixes
that race and NT3.1 P1 passes its condition, audio/video, full 14-program
suite and GUI regressions. NT3.51 and NT4 P1 also pass those regressions.
Win95 P2 passes condition, audio/video, integrated-suite and expanded 38-check
GUI tests with the later presentation scheduler and prepared GDI candidate.
STARTFIX then changes normal first-picture clocking and the Win9x default
clock; W95AN still fails thirteen A/V checks despite observing all 80 scenes.
The stateful fractional resampler now passes J1's 100 checks on all four
guests, while the preceding stateless source fails twelve checks per guest
(RSPROOF.JSN). The combined STARTFIX/GDIPREP2/measured-latency/stateful-resampler
payload passes all seven J2 jobs on all four guests (J2PROOF.JSN): cache,
308 clock/condition checks, 24 audio cases, 82 video checks, the 14-program
suite, 38 frontend/GUI checks and the plugin preflight enumeration. Missing
platform modules remain explicitly unavailable in the enumeration reports.
Captured output remains incomplete: W95J4 fails A/V and small Opus packets;
the optimized resampler's W95J6 capture retains five A/V failures and the
Opus failure, while short clips and bounded continuous tones pass. A
diagnostic Opus repeat W95J9 passes, exposing variability rather than proving
that the problem is resolved. WPROBE's longer calibration queue passes all
three audio-only captures on NT3.1 JC/JD, while A/V still fails. The corrected
GDI subtitle test now passes 25 checks on all four guests (JBPROOF.JSN).
Further W95 traces separate two issues: QEMU's HDA timer model drops audio
on buffer overrun, and the legacy Windows clocks lose time during GDI work
relative to the processor counter and native sound cursor. W95JGHT.LOG and
W95JJCL.JSN retain the evidence. Changing HDA mode alone does not fix the
output (W95JH fails 39 A/V checks). TSCCORE uses a calibrated processor clock
only after detecting a single-CPU TCG guest with TSC support, and extends the
shared output preparation interval to 500 ms. W95JK/JL passes 308 clock/wait
checks, Opus, short clips and bounded continuous tones, but still has two
A/V failures. The same uninstrumented candidate is staged on all four guests
(TSC2STG.JSN). The JP content matrix passes on all four guests, with NT4's
GUI pass established by the separate JR repeat after closing the occluding
Welcome window (N40JRFIX.JSN). JP's NT3.1 run uses a TSC-less 486 and no audio
device: all seven jobs pass, and CPUTEST confirms TSC absent and scalar FFmpeg
flags. JPPROOF.JSN preserves the initial NT4 GUI failure. The closed W95JM/JN callback-mode capture passes all three audio-only
checks but retains three A/V failures. NT3.51 JM/JN passes short clips,
but fails A/V, Opus and bounded continuous tones. AVCLOCK/WCTSC diagnostics
are now staged on the stopped Win95 and NT3.51 disks to separate first
presentation timestamps from clock-rate errors. Actual output acceptance
and the new SB comparison remain pending. MP4TIME then fixes a demonstrated
initial-timestamp error in the native MP4 demuxer: W95JT passes all 80 A/V
scenes with unchanged thresholds and diagnostic binaries (MP4PROOF.JSN).
TSCGUARD rejects a QPC calibration whose scale disagrees with multimedia
time; the NT3.51 rate error is preserved in QPCBIAS.JSN. NT3.51 JT/JU then passes A/V, short clips and continuous tones, while
small-packet Opus retains a pitch sweep (N351JTPF.JSN). SYNCRATE makes the
core correction slope depend on media duration instead of packet count;
957 controlled checks pass while the preceding core fails four cases.
NT3.51's SYNCRATE JV/JW capture still fails Opus, while A/V, short clips
and continuous tones pass. The subsequent MMTSC candidate avoids QPC as
the calibration reference; NT3.51 K1/K2 passes A/V, Opus and short clips but
fails continuous tones, with logs confirming multimedia fallback. Win95
K1/K2 passes Opus and short clips but fails one A/V scene and continuous
tones. Native CLKPROBE diagnostics identify a first-interval phase jump after
timeBeginPeriod; MMTSC2 waits before taking the first endpoint. Its 32
behavioral calibration checks pass, while the preceding source fails the
initial-phase case. MM2STAGE.JSN pins staging on all four systems. The
combined candidate is not yet accepted on all four guests. Final Win32 validation also includes the newly supplied
Win32s 1.30c package on Windows 3.1; its three setup disks are extracted
and checked, but that additional guest test has not been performed.
Earlier unbounded HD reports from mixed QPACK/AOUT captures cannot identify
which test produced the tones. Fresh AOUT checks record and enforce their
own sample bounds; AVSYNC.MD documents that evidence limitation.
X9 below is the preceding four-guest
checkpoint. The complete Win32 port is unfinished; Win16 implementation has
not started. RETRO.ZIP is an older development checkpoint.

| Scope | Current evidence |
| --- | --- |
| 14-program integrated suite on all four guests | N31X9SU / N351X9SU / N40X9SU / W95X9SU: PASS |
| 24 audio cases, including native Xiph and short/chained streams | N31X9AF / N351X9AF / N40X9AF / W95X9AF: PASS |
| 16 extra video/container cases plus AAC | N31X9VF / N351X9VF / N40X9VF / W95X9VF: 82 checks PASS |
| CRT filesystem/time APIs | X9CR reports: 3825 NT / 3824 Win95 checks PASS |
| Guest waveOut path | X9AU reports: 18 checks PASS on all four guests |
| Independent HDA captures with the changed core | N31X9HD / N351X9HD / N40X9HD / W95XAHD.JSN: PASS in isolated runs; W95X9HD.JSN: FAIL under concurrent installer load |
| Installer rollback and validation | All four Y2 INSTEST reports: 86 checks PASS, including the NT3.51 private-directory fix |
| Full installer on all four guests | N31I2OK / N351I2OK / N40I2OK / W95I2OK.PPM; corresponding INH2.JSN reports: all 289 baseline payload hashes match |
| Actual A/V timing | Priority fix resolves MP4 timeout; A7 through AA retain failed startup captures. N31ABCK.JSN passes all 80 scenes against the closed NT3.1 capture; see AVSYNC.MD |
| Plugin availability with PE preflight | 241/254 NT3.1, 243/254 NT3.51, 248/254 NT4, 242/254 Win95; missing platform APIs are not validated features |

All text reports refer to RESULTS/*.TXT. X9SUM.JSN pins the earlier X9 core and
regression reports. Tests use QEMU Pentium/Pentium II/Pentium III models,
not a physical 386. Source changes preserve chained audio when a link ends
before buffering completes, retain the first video picture in no-drop mode,
and flush the final ASF/TS frame. AFORMAT.MD and VFORMAT.MD describe the
independent reference comparisons and earlier failures.

W95X9HD failed the strict captured-tone check while the NT3.1 installer was
running concurrently. Its guest PASS does not override the audio failure.
The isolated repeat W95XAHD.JSN passes with the same core. A fresh SB16 comparison is also
required before packaging these audio/core changes. Earlier HDA/SB successes
remain historical checkpoints, not certification of the changed package.

Full VLC format/dependency coverage, hardware acceleration, A/V synchronization,
network providers, physical hardware execution and the 16-bit implementation
remain incomplete or unverified. No push was performed.

## Verified behavior

| Check | Evidence |
| --- | --- |
| Windows 95 multipart archive | All four parts joined; ZIP CRC passed |
| NT 3.1 archive | Single VHD extracted |
| Original NT3.1/95 disks | Footer checks passed; QEMU recognized VPC; hashes match freshly extracted archives after testing |
| NT 3.1 boot | Logged in and reached Program Manager |
| Windows 95 boot | Reached desktop after hardware discovery and restart |
| NT 3.1 frontend smoke | RESULTS/NT31.TXT: OS=3.10, GUI=PASS, ENGINE=UNAVAILABLE |
| Windows 95 frontend smoke | RESULTS/W95.TXT: OS=4.0 family=9x, GUI=PASS, ENGINE=UNAVAILABLE |
| Normal GUI | RESULTS/NTGUI.PPM and RESULTS/W95GUI.PPM |
| Open without engine | Explicit error on Windows 95; RESULTS/NOENGINE.PPM |
| Cross build | i386 PE32, subsystem 3.10, no CRT imports |
| Package/PE tests | 9 passed; covers frontend/probe only |
| NT4 | Archive/footer passed; boot and frontend smoke passed; RESULTS/NT40.TXT |
| NT3.51 | Installed files verified; bootable FAT16 VHD built; frontend smoke passed; RESULTS/NT351.TXT |
| Atomic runtime | 64 checks passed on NT3.1/3.51/4 and Windows 95 |
| Extended atomic ABI | 104 checks including mixed __sync/__atomic cross-DLL contention pass on all four guests |
| Legacy kernel/UTF-8 runtime | 7300 checks passed on NT3.1/3.51/4 and Windows 95, including affinity and full-path queries |
| CRT runtime | 3778 checks on NT and 3777 on Win95 passed, including aligned allocation, Unicode sharing, environment conversion and 64-bit stat timestamps beyond 2038 |
| SRW/condition variables/InitOnce | 552 contention, timeout, wakeup and publication checks passed on all four guests; RPKERN uses i386 gates and temporary Win32 events |
| GCC emulated TLS | 128 threads, 4096-byte alignment, cross-DLL isolation: 162 checks passed on all four guests |
| Engine initialization | 204 checks passed on NT3.1/3.51/4 and Windows 95, including thread creation, cancellation and cleanup |
| PCM WAV decode | All four guests decoded 16000 samples with byte-exact FNV-1a hash 995135f1; this is callback output, not physical sound |
| Modern software decode | H.264/MP4, H.264/MKV, HEVC/MKV, VP9/WebM: all 12 unique frames match independent host hashes on all four guests; AAC: 96256 samples and amplitude within 1% of host reference |
| Two-worker decode | H.264/MP4, H.264/MKV, HEVC/MKV, VP9/WebM, AV1/MKV and AAC/M4A pass on all four guests; RESULTS/N31AV1.TXT, N351AV1.TXT, N40AV1.TXT and W95AV1.TXT |
| GDI video display | AV1 rendered in all four frontends; RESULTS/N31VIDEO.PPM, N351VID.PPM, N40VIDEO.PPM and W95VIDEO.PPM; low-color display quantization is visible |
| C++ runtime | Strings, containers, RTTI, exceptions, eight concurrent workers, once/retry, deadlines, shared ownership, join/detach and TLS destruction: 20 checks pass on all four guests, including TLS key reuse |
| Supplied NT networking | Winsock 1.1 starts; IPv4 socket creation fails because a usable TCP/IP provider is absent; datagram tests have not passed |
| Windows 3.0/3.1 uploads | Received; signature/archive checks passed; 16-bit work deferred |

Frontend smoke checks are not playback tests. PLAYBACK=UNTESTED is intentional.
The frontend's inspected .text disassembly contains no post-i386 instructions;
this is not a real 80386 test or a guarantee about future dependencies.

## Measured compatibility gaps

| Capability | NT 3.1 | Supplied Windows 95 |
| --- | --- | --- |
| MSVCRT.DLL | Cannot load, error 126 | Cannot load, error 1157 |
| CRTDLL _beginthreadex, _wopen, _wfopen, _fstat64 | Missing | Missing |
| TryEnterCriticalSection | Missing | Missing |
| GetFileSizeEx / CancelIo | Missing | Missing |
| CreateFileW and reading own MZ header | Passed | Failed, error 120: not implemented |
| CreateDIBSection export | Missing | Present |
| StretchDIBits / waveOutOpen exports | Present | Present |
| DDRAW / D3D9 / DXVA2 / D3D11 DLLs | Cannot load | Cannot load |

Export presence is not evidence of functional audio/video hardware. The file
probe checks real read results and treats both NULL and INVALID_HANDLE_VALUE
as failure, rather than trusting a CreateFileW export alone.

## Upstream evidence

At commit 6de05adcbaf2e8b85fe86aad4169393098628119:

- configure.ac:262 defaults to Win32 API level 0x0502.
- extras/package/win32/build.sh:312 documents the XP baseline.
- src/win32/thread.c:156 calls TryEnterCriticalSection directly.
- src/win32/thread.c:506 uses _beginthreadex.
- src/win32/filesystem.c:126 uses CreateFileW.
- modules/video_output/win32/wingdi.c:296 uses CreateDIBSection.
- modules/audio_output/waveout.c uses Unicode device information paths.

An initial VideoLAN download-domain HTTPS CONNECT request returned 403.
The official GitHub mirror was accessible, so source inspection proceeded.
The engine source was exported to VLCWORK. The diagnostic core and LibVLC
now compile using CRTDLL, RPATOM, RPKERN and RPCRT. The core DLL is named
LIBCORE.DLL for DOS 8.3 compatibility. Source changes are saved in
PATCHES/LEGACY.DIF; PREP.SH reapplies them to the pinned source.
COREIMP.JSN records zero unresolved static imports against the supplied
NT3.1 DLL exports; W95IMP.JSN also reports zero missing static exports.
RPNET supplies synchronous Winsock compatibility, and
optional shell and cryptography APIs are resolved at runtime. Export checks
do not prove runtime behavior. Core initialization and a small PCM WAV
decode path pass on all four guests. The separate MEDBLD profile links FFmpeg 8.1.2 software codecs and has passed
the small modern-media suite on all four guests. The default CRTBLD profile
remains a minimal diagnostic. Physical audio, full format coverage and
acceleration are still unverified or unfinished. The separate FFTHR build
uses w32threads and passes the two-worker media tests on all four guests.

The first initialization failure was narrowed to MinGW's prebuilt tfind
routine, whose CMOV instruction is unavailable on the test Pentium CPU.
Other prebuilt support routines also contain instructions newer than i386.
MWCRT.SH rebuilds the MinGW support libraries from pinned v12.0.0 source;
GCC386.SH/GPP386.SH select these objects, including libcrtdll.a support code.
RPCRT now provides GCC's emulated TLS ABI without CMPXCHG or native PE TLS.
CORECPU.JSN scans the current core/runtime/diagnostic modules with no known
post-i386 instruction findings; intentional UD2 exception traps are listed
separately. This is not a real 80386 hardware test or a formal disassembler
proof. Merely passing -march=i386 to application compilation is insufficient.

Windows 95 rejected the native PE TLS directory generated by MinGW, before
executing the DLL entry point (error 1114). A controlled diagnostic changed
one PE field at a time: clearing TLS loaded, clearing DLL flags did not, and
removing the entry point did not. PATCHES/MINGW.DIF now removes the native
TLS directory at source and dispatches MinGW's TLS callbacks from DLL
startup. GCC TLS storage belongs to RPCRT. The rebuilt engine passed both
core and PCM tests again on all four guests. Production executables currently
use the CRT-free frontend entry point; arbitrary CRT EXE TLS notifications
are not covered by the DLL startup change.

RESULTS/NT31PCM.TXT, N351PCM.TXT, NT40PCM.TXT and W95PCM.TXT retain the exact WAV
sample/hash checks. The network test reports BLOCKED (exit 77) only when a
known native IPv4-provider error prevents reaching the RPNET I/O wrappers;
the suite retains BLOCKED distinctly from PASS and FAIL.

## Modern codec integration

FFCORE.SH builds pinned FFmpeg n8.1.2 with software decoding; it currently
disables assembly, codec worker threads, external codec dependencies, encoding
and muxing. This is not the requested full feature profile. COREST.SH accepts
RP_CORE_DIR=/workspace/MEDBLD, RP_CORE_MEDIA=1 and RP_CORE_OUT=BUILD/MEDIA.
SCRIPTS/SAMPLES.PY generates owned media and hashes decoded by the independent host
FFmpeg. MEDTEST.C compares every unique video frame, exact AAC sample count
and an amplitude-sum tolerance of 1%. It does not test audio hardware.

The last-frame test exposed an EOF race: vout_IsEmpty checked decoder_fifo
without counting the prepared display picture. The source patch publishes an
atomic pending flag before consuming the FIFO and retains it until the queued
picture is displayed. The full core/PCM/modern suite passed after this fix.

Additional runtime APIs include _mktime32, _wsopen, _wgetenv, _waccess,
_wrmdir, _stat64/_wstat64/_fstat64 and GetProcessAffinityMask. The latter queries
the native NT process information when NT3.1 lacks the Win32 export.
The GDI module uses StretchDIBits and owned picture memory without requiring
CreateDIBSection. WaveOut builds with a Win9x ANSI device-capability fallback,
but actual device output has not been tested.

## Remaining work

1. Port VLC and its runtime/dependencies: CRT, atomics, threading/cancellation,
   filesystem, Unicode and plugin loading.
2. Coordinate 8.3 names across imports, build rules, plugin lookup and caches.
3. Validate actual decoder/demuxer/subtitle coverage, audio/video sync, seeking,
   malformed inputs and memory limits on the accepted guest systems.
4. Implement and test every acceleration path feasible on these platforms.
5. Extend the four-guest media suite to subtitles, seeking, output devices
   and the remaining formats. More OS images are not a prerequisite.
6. Only after 32-bit completion, implement/test Windows 3.0 real/standard/
   enhanced and Windows 3.1 standard/enhanced modes.
7. Validate i386 requirements and document device/performance limits honestly.

The original images and the unrelated Acik-Kapi-Oyunu checkout were preserved.
Guest changes went into working copies. No push, remote repository creation
or environment publication has been performed.


## Worker threads and AV1

RPKERN now implements SRW shared/exclusive locks, condition variables and
InitOnce using an i386 metadata gate, temporary event waiters and native
Win32 heap allocation. SYNCTEST passed 552 checks on all four guests. Each
condition waiter removes itself under the same gate used by wake operations,
so timeout cleanup cannot close an event while a waker is signaling it.

FFmpeg's CRTDLL worker uses a start gate and a duplicated join handle. The
legacy UDP worker uses nonblocking receive and bounded readiness polling to
replace CancelIoEx; actual UDP I/O remains untested on the supplied images.

DAVCORE.SH ports dav1d 1.5.4 (commit
54706fc6bc0cdecab7e9593974a4039cc038fca7), the version selected by VLC's contrib
rules. It disables unconditional SSE2 in portable C and uses process affinity
on legacy Windows. Decoder tools are excluded; their stdio seek dependencies
have not been ported. Native VLC dav1d integration builds successfully.
The new BUILD/SMP package requests two worker threads for both FFmpeg and
dav1d. Its six synthetic media cases, including AV1/MKV, pass all 38 checks
on NT3.1, NT3.51, NT4 and Windows 95. The full six-program compatibility suite
also passes on each guest. Reports distinguish callback decoding from output.

The GDI output now converts visible RGB32 pixels to positive-height 24-bit
DIBs. This avoids a black window with the original Win95 display driver.
The frontend records PLAYER.LOG to diagnose module/output failures. The four
guest screenshots show AV1 output; their low-color display modes quantize
colors and are not pixel-exact RGB validation. No physical audio result is
claimed.

## C++ dependencies and broader modules

GCC 14.2.0 runtime source comes from the signed Debian gcc-14-source package
14.2.0-19, SHA256 44e6872715a380fe08418ea42b58d3c90de7d5dfd9f525bbf4dacba415c2266a.
CXXPREP.SH applies PATCHES/GCC.DIF; CXXCORE.SH and UNWIND.SH rebuild libstdc++
and DWARF unwinding for i386/CRTDLL. The compiler's original unwind objects
contained unguarded CMOV instructions. Random-device RDRAND/RDSEED paths are
still CPU-dispatched upstream code; CXXRAW.JSN lists those instructions and
also finds non-code exception tables embedded in .text. This is not a zero-
finding or real-386 validation claim.

RPCRT now owns the shared GCC Win32 gthread ABI, CRTDLL worker startup,
condition deadlines, key destructors and descriptor lifetimes. C++ call_once
keeps throwing callbacks in a C++ frame and resets a failed attempt. The
chrono port uses native FILETIME and QPC instead of the one-second time()
fallback. GetModuleHandleExW supports name/address reference acquisition;
permanent PIN is explicitly unsupported on the old kernels. rand_s uses a
native cryptographic provider when available and returns ENOSYS otherwise.
No timer-based substitute is represented as cryptographic randomness.

SRC/SYNCABI.H works around GCC 14 dropping __sync_lock_release on an i386
64-bit operand and routes lock builtins through the shared __atomic gate.

The broader diagnostic currently compiles 245 VLC modules. NT3.1 static
import checks identify 13 modules requiring unavailable COM, DirectX,
display or encryption APIs. These modules are not validated features.
BUILD/BROAD is an experimental package, not a release or replacement for
the tested BUILD/SMP profile. The updated plugin-error path bounds message
formatting and falls back to ANSI on Win95. WSAIoctl covers synchronous
FIONBIO/FIONREAD/SIOCATMARK; other operations return unsupported errors.

## Compression, text and loader work in progress

DEPPREP.SH verifies bundled zlib 1.3.2, FreeType 2.14.3, FriBidi 1.0.12,
HarfBuzz 14.2.1 and libxml2 2.15.3 sources. DEPCORE.SH builds static i386
libraries. FFCORE.SH now enables zlib when that prefix contains it, enabling
additional compressed codecs including PNG. The FreeType patch uses a C-only RPSetJmp/RPLongJmp pair; NT3.1 CRTDLL
assumes a different SEH frame layout. The libxml2 patch uses RPCRT TLS-key destructors
instead of RegisterWaitForSingleObject. Its dictionary hash seed prefers OS
entropy; systems without a provider use a documented weak address/time seed,
as upstream does on POSIX without getentropy. This is not cryptographic RNG.

DEPTST.SH covers compression, checksum and damaged data, gzip wide opening,
XML/XPath, invalid XML, eight concurrent parsing workers, Hebrew bidi, font
rasterization and Latin/Arabic shaping. The initial 58-check integration test
passes on NT3.51, NT4 and Win95 (N351DEPS/N40DEPS/W95DEPS.TXT). NT3.1 also reaches all 58 checks after the C-only jump adapter; its
new report and the full text-profile suite have been observed in guest
Notepad and are being exported. The extended
20-check C++ test now passes on all four systems, including TLS key reuse.

BUILD/TEXT contains 247 compiled modules, including the new FreeType and XML
plugins. RPFONTS.c discovers bundled and system fonts without DirectWrite or
Uniscribe. Real subtitle rendering and the full package remain under test.
The original broad package exposed a legacy NT loader reference-count failure
after failed optional-plugin imports. PLOAD showed RPATOM disappearing while
LIBCORE remained mapped. Skipping unresolved plugins prevented this in the
isolated NT3.51/NT4 diagnostic. RPLOAD.H now validates PE static imports before
loading plugins; full playback/unload regression results are still pending.
PLOAD reports COMPLETE for finished enumeration, not full plugin availability.

New _wopen/_wfopen/_wstat32 adapters passed 3808 checks on NT3.1/3.51/4.
Win95 exposed four failures caused by CRTDLL rejecting CREAT|EXCL|TRUNC.
The adapter now removes redundant TRUNC during exclusive creation.
The corrected Win95 CRT test passes 3809 checks, including both nonlocal
jump return-value cases (W95CRT5.TXT). Earlier passing CRT reports predate these new tests. None of these
experimental findings changes the independently tested BUILD/SMP checkpoint.

The new PE preflight and text profile pass the complete media suite on
NT3.51 and NT4 (N351TEXT/N40TEXT.TXT). All 247 modules were enumerated without
losing the live core dependency in the guarded NT31/351/4 diagnostic, with
234/236/241 loadable modules respectively. This is module loading coverage,
not playback coverage for every module. The standalone Win95 guard also completes enumeration after the ordinal-export fix: 235 loaded and 12 unavailable (W95PGR4.TXT).

Win95 hides certain private KERNEL32 ordinal exports from GetProcAddress.
The preflight now reads the export table of the actual loaded module for
these Win9x ordinal-only cases, without reinitializing its dependencies.
The standalone Win95 regression passed. RUN.PY
also waits for HMP modifier-key release before typing guest commands.


## Assembly, character conversion and cache integration

The experimental BUILD/ASM profile builds FFmpeg assembly with scalar C still
restricted to i386. CPUID dispatch keeps MMX/CMOV capabilities but masks all
SSE/AVX families when the OS cannot preserve XMM state. IsProcessorFeaturePresent
is resolved dynamically; missing support fails closed. NT3.51 on an emulated
Pentium III reports SSE in CPUID, but the OS gate correctly removes it. The
CPUTEST reports N31CPU2/N351CPU2/N40CPU2 and W95CPU1 verify dispatch, not physical
acceleration or real-386 execution. Dav1d assembly remains disabled.

GNU libiconv 1.17 is now included from the pinned source mirror recorded in
SOURCES/DEPS.JSN. ICONV.SH generates upstream converter tables and builds the
static library with the legacy CRT and i386 flags. The library's complete
encoding tables are retained, including the optional encodings. The expanded
94-check dependency test passes on all four guests. It includes UTF-32 output,
UTF-16 surrogate pairs, embedded NULs, CP1254, Shift-JIS, ISO-2022-JP, invalid
sequences, incomplete input, buffer exhaustion and descriptor reset.

The missing iconv dependency caused the previous subtitle failure: parsing and
SPU queuing worked, but FreeType's UTF-8-to-UCS-4 conversion returned NULL.
N31SUB1/N351SUB1/N40SUB1.TXT now pass 266 checks: the identical solid video
has zero bright pixels with subtitles disabled and 5867 with them enabled.
The corresponding PPM captures show English, Turkish and shaped Arabic text.
These are actual subtitle pixels from VLC's video callback. W95SUB1 also passes 266 checks. GDI subtitle output is now captured on all four guests: N31SGDI, N351SGD2, N40SGD2 and W95SGDI.PPM. NT3.51/NT4's first replay captures ended before showing subtitles and are not passing visual evidence.

N31ASM1/N351ASM1/N40ASM1.TXT pass the 11-program integrated assembly/text/cache
suite. All modern-media reference hashes and AAC checks pass, as do lossless
RGB PNG and ZMBV comparisons. BUILD/ASM now contains 248 modules, including the
inflate module; compilation is not a claim that every module is usable.

The cache temporary filename is now eight hexadecimal digits plus .TMP, avoiding
upstream's long plugins.dat.PID temporary name. Cache generation and cache-only
reads agree on NT3.1/3.51/4; NT3.1 measured 43 filters in approximately 21 seconds
cold versus 105 ms warm. Win95 initially failed to read the generated cache: the block reader used OVERLAPPED offsets on a normal synchronous file. The legacy branch now seeks and reads synchronously on Win9x. Unexpected EOF also terminates with EIO instead of spinning if a file shrinks after fstat. W95CACH7 passes 12 checks (9597 ms cold, 178 ms warm); W95ASM4 passes the full 12-program suite. W95ASM1/2 retain the earlier failures.
FILETEST adds real VLC stdio, append-with-create preservation, exclusive create,
replacement/missing-source rename, truncation and text translation checks.
W95FILE1 passes all 41 checks after routing legacy descriptors through RPCRT.
The failure was in cache reading, not writing. N31UTF2/N351UTF1/N40UTF1/W95UTF2 pass the subsequent integrated suite. The file test now passes 48 checks on all four guests, including binary block reads and writable private buffers. FreeType's legacy stream opener now converts UTF-8 paths through RPKERN and RPCRT; all four 100-check dependency reports (N31D4/N351D4/N40D4/W95D4) include opening Fé.TTF and rendering its accented glyph.

VMAGENT.EXE and SCRIPTS/VMRUN.PY use a private Unix-backed COM1 connection for
allowlisted test execution and checksummed report retrieval. They expose no
network listener. Disk deployment still requires clean shutdown and VMIO.PY;
original uploaded OS disks remain unmodified. GET can read reports while a
suite runs, so only completed reports containing final RESULT lines count as
passing evidence. Full format support, hardware audio/video acceleration,
network providers, seeking/AV sync validation and 16-bit implementation remain
unfinished. No push was performed.


## ASS renderer and directory API integration

ASSCORE.SH builds libass 0.17.5 from the archive hash pinned by VLC's contrib
rules, without assembly or modern OS font providers. The VLC plugin now adds
the bundled DejaVu font; existing attachment loading is preserved. This font
fallback does not yet discover every system font. The expanded ASSVID test
requires the real libass decoder and visible pixels from styled UTF-8 ASS.
The 267-check test passes on NT3.1/3.51/4 and Win95: N31ASS4,
N351ASS2, N40ASS2 and W95ASS2.TXT. Matching PPM captures contain 899 bright
pixels with ASS enabled versus zero without it. The test explicitly disables
subtitle autodetection: otherwise the adjacent SRT can take precedence over
the requested ASS and produce a misleading image. N31AS5, N351AS3, N40AS3 and
W95AS2.TXT pass the full 13-program suite with the previous 7351-check kernel.
The current ASM profile contains 249 plugins; module count is not format
coverage.
The iconv header now uses the conventional POSIX char ** input argument for
source compatibility; the binary calling convention is unchanged.


RPKERN now supplies wide directory enumeration on Win9x, FindFirstFileExW
basic information and directory filtering across subsequent FindNextFileW
calls. Lossy filename conversions are rejected. Native NT wide enumeration
is retained; case-sensitive search requests return ERROR_NOT_SUPPORTED.
AreFileApisANSI and the ANSI/OEM setters use native exports where available.
NT3.1 lacks the query and ANSI setter: the adapter recognizes the OEM setter's
instruction and validates its referenced writable image section before
accessing the process-local mode byte. Unrecognized kernel layouts return
ERROR_CALL_NOT_IMPLEMENTED; the mode query then conservatively returns TRUE.
The NT3.1 images leave section VirtualSize zero, so validation also considers
SizeOfRawData. N31KF3/N351KF3/N40KF3/W95KF3.TXT pass 7353 checks, including
switching to OEM and back, native-handle closing and accented FAT filenames.
The latter can be uppercased by NT3.1 FAT, which is accepted by the test.

SEEKTEST adds a generated 30-second H.264 MP4 with 60 unique frames and
independent host-decoded hashes. Pause/resume, forward/backward seeking,
end-of-media and stop/play replay pass 35 checks on all four systems at the
displayed-frame callback (N31SEEK1, N351SK1, N40SEEK1, W95SEEK1.TXT). Every
observed frame matches a reference; seeks reach frames 40, 6 and 56 as expected.
All four guests also pass the complete 14-program suite with the current kernel
(N31SK2/N351SK2/N40SK2/W95SK2.TXT). This is not physical A/V synchronization coverage. The frontend
now exposes backward/forward ten seconds and beginning commands through the
Playback menu. The frontend controls now pass on NT3.1/NT3.51/NT4/Win95; see the UI evidence below.


The frontend now uses the native Unicode file dialog on NT and the ANSI dialog
on Win9x, with validated UTF-16-to-UTF-8 conversion before LibVLC. Cancelling
a file dialog preserves the current loaded-media state. UITEST exercises the
actual file dialog with an accented temporary media filename (an Omega outside
the ANSI code page on NT4), paused seeking and GDI pixels, end/replay, and
cancel/stop/replay. The expanded 30-check UI test passes on NT3.1/NT3.51/Win95
(N31UI2/N351UI3/W95UI7.TXT). It checks new GDI output after replay by seeking
again, rather than accepting a cleared black window. Win95's first new frame
arrived 1200 ms after Playing; a fixed one-second assertion was too early.
N40UI1.TXT retains the earlier failed NT4 selection diagnostic. NT4's dialog
exposed its edit before folder initialization finished and cleared the early
selection. After waiting for initialization and checking the edit text, the
31-check test passes (N40UI2.TXT), including the Omega filename outside the
ANSI code page. N40ULOG2.TXT records the actual Unicode file input. The final
harness also records IsWindowUnicode and readback length. The three earlier
30-check reports predate that additional edit-readback assertion; the tested
frontend executable is identical across all four systems.
