# Independent audiovisual timing checks

Full A/V synchronization coverage is unfinished. Earlier NT3.1 MP4 stalls
were fixed and subsequent NT3.1/NT3.51 captures passed. STARTFIX with the
stateless resampler still fails W95ANCK despite observing all 80 scenes.
A stateful fractional resampler now passes 100 source/allocator checks on
each of the four guests, with the preceding implementation failing twelve
checks on each guest. Its playback regressions and captured output are under
test. Historical failures and intermediate candidates below remain preserved;
no experimental candidate is accepted merely because it builds.

AVTEST.SH builds AVTEST.EXE and a separate AVFTEST.EXE diagnostic variant which
explicitly selects avformat. Both use the same owned H.264/AAC MP4 and
HEVC/Opus Matroska fixtures. Each file contains 40 numbered half-second scenes
and one 100 ms sound pulse at the beginning of each scene. AVREFS.JSN pins the
encoded inputs. The normal harness uses automatic demux selection.

The harness displays actual RPGDI output in a 384 by 288 window and sends audio
through waveOut and the guest HDA driver. The observer reads screen pixels and
the host WAV sink's sample cursor. Serial status queries run separately so an
old guest's delayed reply cannot suspend screenshot sampling. Decoder logs
are buffered in bounded memory and written after playback, avoiding disk I/O
from real-time callbacks. Log truncation is an explicit test failure.

After a clean shutdown, AVCHK.PY compares all 80 scene transitions against the
closed sound capture. Missing, reordered or repeated scenes, missing pulses,
incorrect pulse spacing/duration and timing outside the bounds fail. The
150 ms bound includes the measured screenshot interval, one-sided stdio
buffering uncertainty (8192 bytes), 20 ms sink scheduling allowance and a
4 ms detection window. Positive offsets mean sound follows the image.
These are emulated outputs; the test does not certify physical speakers,
displays, arbitrary resolutions, every codec or seeking synchronization.

TESTS/AVCHK.PY uses synthetic recordings to verify a matching sequence passes
and a 300 ms offset or missing scene fails. AVCPASS/AVCDRFT/AVCMISS.JSN are
analyzer self-checks, not guest evidence.

N31A1 retains the initial failed experiment: synchronous serial polling missed
scene transitions, and the harness looked for a lowercase module name although
the actual module is RPGDI. N31A2 corrects these measurement problems and still
fails: H.264/AAC MP4 remains Playing after the 90-second limit instead of
finishing its 20-second presentation. HEVC/Opus finishes, with 39 of 40 scene
timing bounds passing; one upper bound is 150.93 ms and is kept as a failure.
The MP4 regression is being isolated with the explicit avformat comparison.

## Priority-gate and arithmetic investigation

A3 still failed when the same MP4 used avformat. A4 used 64 MiB and reported
at least 25532 KiB available physical memory during MP4 playback; it still
timed out. A5 explicitly selected aac_fixed and reached Ended, but its
independent capture still failed. These diagnostic overrides are not the
normal playback configuration.

The resampler used 32-bit products for both output sample counts and duration.
Long buffers could wrap even at ordinary sample rates. The fixed code widens
those calculations and rejects output sizes that cannot be allocated safely.
N31R1RS.TXT passes 55 checks against the actual source and real block allocator.
N31R1OLD.TXT fails 14 of the same checks against the unchanged upstream code.
This check covers long-buffer arithmetic, not fractional phase continuity.

A6 subsequently froze in the RPATOM contention loop. N31A6HNG.JSN records the
CPU in NtDelayExecution, a zero timeout on its stack, and a caller inside
__atomic_fetch_or_4's Sleep(0) loop. On these Windows versions Sleep(0) does
not yield to a lower-priority lock owner. Both the atomic gate and the private
synchronization gate now use Sleep(1) on contention. PRITEST uses the actual
source to preempt a lower-priority owner and tests bounded completion; its
higher-priority watchdog restores scheduling even for the old implementation.

A6 observation aborted after the serial timeout. The guest was recovered and
shut down, but debugger recovery was attempted and its modified-byte count was
not confirmed. N31A6REC.JSN records this limitation. A6 cannot be acceptance
evidence. Fresh boots with the rebuilt DLLs are required.

N31P1PR.TXT now passes all 19 priority checks: both gates finish in 15 ms.
N31P1OLD.TXT reproduces both one-second timeouts and recovers through the
watchdog. N31P1AT.TXT passes the 104-check cross-DLL atomic test with 40000
updates, compare/exchanges and 64-bit operations, without torn values.

A7 uses the normal decoder selection with rebuilt gates and finishes both
files on NT3.1. All 80 video scenes are present and all 40 HEVC/Opus timing
bounds pass. AAC's first audible pulse is split by about 30 ms of zero output;
the complete A7 capture still fails. The waveOut startup path is being changed
to queue leading PCM silence instead of sleeping before its first write, so
the decoder can queue subsequent packets before sound starts. Buffer counts
are also reserved before submission, and the completed block is no longer
accessed after cleanup can release it. A8 repeats the same first-AAC-pulse split with those output changes at 32 MiB, while both files still finish and all video scenes are observed. Leading silence has not fixed the split. A decoded-PCM callback diagnostic is being used to separate decoder output from device scheduling; the timing tolerance remains unchanged.


N31P3AV.TXT passes the corrected decoded-PCM diagnostic. Its 441344 samples
are identical to N31P2PCM.RAW: the first decoded pulse is continuous. P2's
harness incorrectly asserted volume support for amem and treated the initial
waveOut configuration as active hardware output; those assertions failed and
remain recorded. P3 checks the actual amem selection and omits that unsupported
volume operation. It does not constitute a device-output pass.

N31Y1 reports pass all ten NT3.1 guest regression programs; N31Y1HD.JSN also
passes the independent continuous-tone analysis of the closed N31A8SN.WAV.
The final N31A8CK.JSN remains FAIL for A/V startup. A9 instruments the first
waveOut calls: the first AAC buffer has 1024 samples (46.44 ms), but the second
call arrives 75 ms later with no queued buffer remaining. The first presentation
date is already 5 ms late. Opus also initially exhausts its short first buffer.

The startup candidate gives legacy audio and video a shared 150 ms scheduling
lead after decoder buffering, replacing the upstream 10 ms wakeup assumption.
Together with nonblocking queued PCM silence, this lets the decoder prepare
more data before the first audible sample. STARTBLD.JSN pins the candidate;
fresh A/V and four-guest regression results are required.


AA retains the failed leading-silence experiment: N31AACK.JSN has 81 pulses,
including the split AAC onset. The current implementation instead pauses the
wave device, queues at least 100 ms of audio, waits for the shared presentation
date, then restarts it. Drain explicitly starts clips shorter than that
threshold, and flush resets the startup state. Drivers unable to pause retain
the previous timing fallback and emit a diagnostic warning. QUEUEBLD.JSN pins
the first candidate. The AB live-file snapshot passes all 80 pulses/scenes;
closed-file acceptance and four-guest regression remain pending.

QUTEST and QUCHK exercise 5, 20, 70, 100 and 200 ms PCM clips. The analyzer
checks all five pulse durations; frequency is measured only for pulses at
least 25 ms long. QCPASS/QCLOSS/QCSHORT/QCEXTRA are synthetic analyzer checks,
not guest playback. The first NT3.1 harness run Q1 exited 128 during engine
loading before producing an audio log; it is a failed test, not a drain pass.


Q2 proves the engine loads before the watchdog thread creation fails with
exit 128. Supplying a real DWORD output for CreateThread's thread identifier
fixes the old-Windows API call; Q3 passes all 37 guest checks. Its independent
live-capture preview N31Q3PC.JSN measures 5.99, 19.95, 69.84, 100.77 and
200.54 ms pulses in the requested order. This is a harness correction, not a
media-engine loading fix. Q1/Q2 remain retained failures.


The closed N31ABSN.WAV now passes N31ABCK.JSN (80 pulses and 80 video scenes),
N31Y2QD.JSN (all five short clips) and N31Y2HD.JSN (three continuous tones).
N31Y2.JSN records all eleven guest programs exiting successfully. These are
NT3.1 emulated-device results, not a complete Win32 release. Y2FILES.JSN found
four older diagnostic EXEs on NT3.1; the current copies are being deployed
and their integrated suite will be repeated. The NT3.1 engine/runtime/plugin bytes matched. NT3.51 matched all checked
files. The same audit found older plugin binaries on NT4 and Win95; those
are being fully replaced before the fresh regression. Other operating
systems still need the same playback regression.

The same queued-start payload passes the closed NT3.51 capture in
N351ABCK.JSN: 80 pulses and 80 observed scenes meet the unchanged timing
bounds. N351Y2QD.JSN passes all five short clips, N351Y2HD.JSN passes
continuous-tone quality, and all twelve N351Y2 guest programs exit zero.
These are QEMU HDA output checks, not physical hardware measurements.

## NT4 queued-start result and first-picture candidate

N40Y2 passes all twelve guest programs, and the closed N40ABSN.WAV passes
short clips and continuous tones (N40Y2QD/N40Y2HD.JSN). Its A/V check fails:
N40ABCK.JSN observes all 80 scenes/pulses but finds the first HEVC picture
199–317 ms ahead of its audio and one pulse spacing at 458.96 ms, outside
the unchanged 460–540 ms bound. This is not a four-guest A/V pass.

The decoder log shows that the forced first-picture preview is displayed
before decoder buffering completes (325 ms for HEVC on this run). A separate
BUILD/VSTART core candidate acknowledges and holds the first normal-playback
picture until the shared clock is aligned. Paused seek previews retain their
existing behavior. Its initial SHA256 is
0e10661cdc3043452b4a7c24a2b26ca95bdf820195e893fde80bbaae57792256.
It is staged only on NT4 for a focused repeat; BUILD/ASM and the installer
remain at the queued-start checkpoint. Guest validation is pending.

Win95 AB fails independently: 80 audio pulses but only 77 observed video
scenes and 43 failed checks (W95ABCK.JSN). The HEVC guest run records 196
waveOutWrite errors and very low available physical memory. All twelve
W95Y2 guest regression programs still exit zero, so that matrix cannot
substitute for the A/V result.

The WINAUDIO driver source has a fixed 32-header application queue; VLC
can submit two seconds ahead, exceeding that queue with 20 ms packets.
BUILD/WQUEUE is a separate candidate that limits outstanding writes to 16
and gathers the initial 100 ms of PCM before the paused device starts.
This avoids needing forty queue entries for 2.5 ms codec packets. Short-clip
drain submits whatever is buffered; flush/stop discard pending data; pending
pause/resume adjusts its timestamp. The candidate also reports WinMM error
codes and unprepares headers after failed writes. Guest validation is pending.
AVCAND.JSN pins both candidates and the preceding ASM versions.

The first-picture core alone fails NT4 AC: 80 detected pulses but 76 video
scenes, with a 1.5-second scheduling interruption and audio glitches. The
functional suites pass; N40Z1UI initially fails because the Welcome window
obscures pixel reads, and N40Z2UI passes all 31 checks after closing it.
NT4 AD repeats the output measurements with Welcome closed.

Win95 AC with the first bounded-queue candidate removes waveOutWrite errors
and shows all 80 scenes, but still fails eight timing checks (W95ACCK.JSN).
Short clips and continuous tones pass (W95Z1QD/W95Z1HD.JSN). QPACK exercises
2.5/20/60 ms native Opus packets: its 27 guest checks pass but the independent
nine-tone capture fails (W95Z1PD.JSN). The 2.5 ms stream has late-buffer drops;
sixteen such headers cover only 40 ms. The next WQUEUE candidate coalesces
ordinary PCM writes to at least 20 ms as well as the initial 100 ms, and
includes held PCM in its delay calculation. SHA256:
6ede6444c59c47d51fb7e16c45177ffc3ed1fbc2ea26ded2c1338891449efa96.
It is staged on Win95; NT4 AD still uses the initial 89cbed15 candidate.

CLKTST measures scheduler waits without asserting a hardware speed guarantee.
N40CLK1.TXT observes roughly 10 ms waits for Sleep(1), both before and after
opening waveOut. Requesting the supported 1 ms multimedia timer period reduces
the median to about 1 ms. The diagnostic restores that request afterward; no
production timer-resolution change has been made.

W95Z2 uses the 20 ms coalescing candidate. Its closed W95ADSN.WAV passes
short clips (W95Z2QD) and standard continuous tones (W95Z2HD), but the three
Opus packet-size sequences still fail (W95Z2PD). Guest QPACK returns zero;
that does not override the captured-audio failure. The logs show 61–69 ms
startup drift followed by active resampling. Spectral inspection observes
the nominal 441 Hz tone rising to about 456 Hz in the 2.5 ms stream.
W95CLK1 also observes Sleep(1) medians near 7 ms with waveOut open and
about 1.5 ms while requesting a 1 ms multimedia period.

The first-picture candidate is rejected after NT4 AC/AD output regressions.
PATCHES/FIRSTPIC.DIF and RESULTS/FIRSTPIC.JSN preserve the exact change and
candidate hash. The decoder source again preserves upstream preview/warmup
ordering; Z3STAGE.JSN restores the verified f50d03b7 core bytes on NT4/Win95.
The rebuilt source in BUILD/VRESTORE has a different whole-file hash:
VRESTORE.JSN confirms every PE section is byte-identical to the baseline;
only the PE build timestamp and its dependent checksum differ. Deployment
still uses the exact previously tested binary.

AV2TEST adds a separate one-second black/silent lead before all forty marked
transitions in each fixture. AVRUN --lead records these fixture hashes and
uses the unchanged AVCHK limits for all eighty transitions. This distinguishes
scheduled playback from the upstream forced initial-frame preview. The
original fixtures and failed startup observations remain preserved; AV2 is
not evidence that the original first-frame timing problem is fixed.
POSTEST separately records actual WinMM sample positions against QPC after
a paused queue starts. It makes no latency or real-hardware pass claim.

W95POS1 observes first positive sample positions around 60 ms on the first
start and 71 ms after reset. The position continues about 60–75 ms behind
elapsed time. W95 AE (20 ms coalescing, original core, AV2 fixtures) sees all
80 scenes/pulses but fails 18 timing checks in its closed capture. Audio
correction changes the pulse cadence and eventually leads video.

The next waveOut candidate measures the opened PCM device using a silent
100 ms buffer during initial input buffering. It estimates startup latency
from elapsed time minus played sample time, resets the probe and schedules
the queued media start that much earlier. The embedded probe header is kept
separate from VLC block completion accounting. Unprepare failures retain
driver-owned memory until successful close. Encoded S/PDIF skips this PCM
probe. WLATST.JSN pins the candidate; output and lifecycle regression are
pending. No fixed HDA latency or Sound Blaster assumption is built in.

The first calibration candidate passes NT4's closed N40AESN.WAV: N40AECK
passes all eighty AV2 transitions, N40Z4PD passes all nine Opus packet-size
tones, N40Z4QD passes short clips and N40Z4HD passes standard tones. S4/S5
also pass Speex, existing audio/video and GUI regressions. These are isolated
QEMU HDA measurements with AUDIONT, not physical-device certification.

N40POS1 also exposes a cold/warm difference: first startup advances nearly
immediately, whereas restart after reset has about 55 ms latency. The first
calibration log includes a zero estimate for cold playback. It also shows
false early-audio correction while the sample cursor is still zero. The
second candidate probes twice to measure the warm reset path, and withholds
position feedback until the cursor advances. SOURCES/WLAT1.C and
RESULTS/WLAT1.DLL preserve the first implementation; WLAT2ST.JSN records
the updated candidate on NT4/Win95, and N31WL2ST.JSN records NT3.1 staging.
The second candidate still requires its own output regression.

Win95's second candidate passes all three independent sound-only checks in
the closed W95AFSN.WAV (W95Z4PD/QD/HD): nine Opus tones, five short clips and
the normal tone triad. Its GUI test also passes. W95AFCK still fails eighteen
A/V checks and misses two video scenes. The logs no longer show the earlier
audio resampling correction; they show delayed video presentation.

A separate BUILD/GATE runtime candidate replaces only the internal RPATOM
and RPKERN metadata gates with native blocking critical sections. These
private objects link directly to KERNEL32; the public RPKERN critical-section
ABI remains mutex-backed. The change avoids Sleep(1) polling at a coarse
scheduler resolution. SRW acquisition and InitOnce polling are unchanged.
SOURCES/ATGATE.C, SYGATE.C and KEGATE.C preserve the earlier runtime sources;
PRITEST still builds the known Sleep(0) priority-starvation comparison from
that archive. The new runtime is not integrated into ASM or RPLIB, and its
priority, atomic, synchronization, content and output validation is pending.

NT3.1 passes the second waveOut candidate with the existing runtime gates:
the closed N31AESN.WAV passes N31AECK (all eighty AV2 transitions), N31Z4PD
(all nine Opus tones), N31Z4QD (short clips) and N31Z4HD (standard tones).
N31Z5UI also passes all 31 GUI checks. N31AEAL records a cold estimate near
zero followed by a warm estimate near 55 ms, confirming why the second
probe is needed on AUDIONT. These results do not cover the new gate candidate.

The native-gate experiment passes Win95 priority (19), atomic (104), the
14-program integrated suite, existing audio/video, GUI and plugin-loader
checks. It also preserves all captured sound-only checks. It does not fix
A/V: W95AGCK fails and four video scenes are missing. It is not accepted as
the solution; SRC and the Win95 runtime were restored to the verified gates.
ATGATE2.C/SYGATE2.C/KEGATE2.C retain the experimental sources, and GATE.SH
can reproduce them separately. No ASM/RPLIB runtime was replaced.

Source review found that the core still looks for condition-variable APIs in
KERNEL32. On the target systems this selects DummyConditionVariable and its
5 ms polling fallback, even though RPKERN already implements real condition
events. BUILD/CVCORE resolves those APIs from RPKERN to match the public CS
ABI, and refuses a missing compatibility API instead of silently polling.
It also corrects mdate_default: after choosing multimedia time, the clock
union contains function pointers and must not be interpreted as a QPC
frequency. CWTEST exercises the first two clock reads, 256 real core
condition exchanges, timeout and sixteen cancellation/lock-cleanup cycles.
Both this test and the full core/media/output regression are pending.

The condition-event candidate passes CWTEST's 308 checks on all four guests.
W95CWOLD fails the first-clock check with a 38423 ms jump; W95CWNEW reads
23 ms and reduces 256 handshakes from 2819 to 88 ms in these runs. NT31,
NT351 and NT40 C2 also pass. This core is not accepted into ASM: N31C2VF
loses the first WMV1 and MJPEG picture, and N40C2's integrated suite loses
the first VP9 picture. CVFRAMES.JSN compares retrieved NT3.1 pixels against
the independent reference and identifies frame 1 in both failures.
NT351C2 and W95C2 pass their content/GUI regressions. W95C1 captured audio
checks pass, but W95AHCK still misses five AV2 scenes. All failures remain
preserved while the display flush/startup path is investigated.

NT4 diagnostic queue tracing confirms the first-picture race (PVCAUSE.JSN):
Theora's forced preview is queued at 230639000, then a partial flush at
230044000 removes it while the display has no current/next/decoded picture.
VP8 and JPEG2000 reproduce the same sequence. COREPRV preserves this queued
initial preview before a partial clock-rebase flush; full seek/stop flushes
still discard it. Its regression matrix is in progress.

W95BLT1 measures direct StretchDIBits at roughly 62 ms for 24-bit input and
42 ms for 4-bit input at 384x288 on the supplied 16-color display, with zero
pixel-check errors. GDIPREP moves scaling to a compatible off-screen bitmap
in Prepare and copies it in Display so VLC can account for preparation time.
The first version fails W95AICK with three missing scenes and 28 total
failures; it is not accepted. SOURCES/GDIPREP1.C and BUILD/GDIPREV preserve
that version. The revised candidate compares every source pixel and output
placement before reusing a prepared surface, avoiding repeated scaling of
unchanged content while still detecting subtitle changes. GDISTG2 pins the
second W95 candidate and the expanded paused-resize GUI test.

NT3.1 P1 passes the preview candidate's 308 clock/condition checks, 24 audio
cases, 82 video checks, the 14-program integrated suite and the 31-check GUI
test. NT4/NT3.51 were staged with the same COREPRV binary for their regressions.
W95D2UI passes the second GDI candidate's expanded 38-check UI test: the
paused forward-seek image remains at luminance 128 after shrinking and
restoring the actual frontend window. A/V acceptance remains separate.

The exact-pixel GDI cache brings the next Win95 run down to approximately
53 seconds for both fixtures and captures all 80 scenes (W95AJTR). The closed
W95AJSN.WAV still fails W95AJCK: four timing bounds and one 458.96 ms pulse
spacing are outside the unchanged limits. Audio logs also show one early
clock correction; all-scene visibility alone is not A/V acceptance.

BUILD/PRESENT adds a separate synchronous Display-duration estimate on top
of COREPRV. Rendering is scheduled early enough to include both Prepare and
Display; deadline waits subtract the measured copy duration, and redraw
intervals begin at display completion. Only actual callback duration updates
the estimate, excluding deadline waits. PRSTAGE.JSN pins the W95 candidate;
content, output and additional-guest validation are pending.

A sound-analyzer review found an attribution gap in earlier unbounded HD
reports from captures that also contained QPACK. The analyzer selected the
first matching tone triad, which could be an earlier Opus sequence rather
than AOUTTEST. Those HD reports demonstrate tones but are not independent
AOUTTEST evidence. QPACK reports already use their own bounds and are
unaffected. VMTEST now records AOUTTEST's AB bounds, and SNDCHK rejects
ambiguous matches. W95HAMB intentionally rejects the old mixed W95AHSN
capture. Fresh bounded AOUTTEST capture checks are required for the final
payload; no earlier failed or ambiguous report has been overwritten.

W95P2 passes the PRESENT candidate's condition/audio/video/integrated-suite/
38-check GUI/plugin-loader jobs. Its closed W95AKSN.WAV passes the five
short clips (W95P3QD) and the freshly bounded AOUTTEST tones (W95P4HD).
The nine-tone Opus check W95P2PD fails after an early-audio resampling
correction in PACK2; its guest exit remains zero and does not override that
capture failure. AV2 W95AKCK observes all 80 scenes but fails four checks
around an AAC clock correction. The original fixture W95ALCK also observes
all 80 scenes: its only failures are the two forced previews, roughly 200 ms
ahead of their audio. The measured synchronous display copy is about 4 ms.
These candidates are not yet a four-guest output checkpoint.

AVPERF.EXE provides a separate clock-source diagnostic with the AV2 fixtures.
It requests the performance counter and retains a 5 ms multimedia timer
period, so clock source is the intended independent variable. It verifies
that actual LibVLC clock reads include sub-millisecond values. AVRUN --perf
records the override explicitly; this is not the default playback profile.
PFSTAGE.JSN records its W95 deployment.

W95AMCK passes all 80 AV2 scenes against the closed W95AMSN.WAV with the
performance-clock diagnostic. The harness confirms 32 sub-millisecond clock
reads and a successful 5 ms timer request. A later core candidate, STARTFIX,
selects this clock automatically on Win9x when QueryPerformanceFrequency
succeeds, otherwise retaining multimedia time; explicit clock options still
apply. Timer resolution is kept independent of this selection. It also
holds the first normal-playing picture until input buffering has rebased
the shared clock, while paused seeks retain the immediate preview. This
revisits first-picture clocking with the corrected core condition APIs and
new output path; the earlier FIRSTPIC failure is still preserved. SFSTAGE
pins W95 staging. This candidate requires its own normal-profile tests.

The closed W95ANSN.WAV confirms STARTFIX still fails the original A/V fixture:
W95ANCK observes all 80 scenes but fails thirteen checks. The same capture
passes the independently bounded Opus packet, short-clip and continuous-tone
checks (W95S1PD, W95S2QD and W95S2HD). The AAC log shows an initial late-audio
correction followed by excessive early-audio correction. These are retained
failures, not acceptance of STARTFIX.

## Fractional resampling across packet boundaries

The previous arithmetic fix widened products but still rounded each packet
independently. At 48002 -> 48000 Hz, two thousand 120-frame packets produced
238000 frames instead of 239991: about 41.5 ms of unintended sample loss.
This error can magnify a small clock correction and depends on packet size.
The stateful implementation carries the next sample's fractional source
position across packets, rescales it when the output rate changes and resets
it on flush/discontinuity. Empty-output packets remain valid at large
downsampling ratios. Sample indices use the same nearest-neighbor rule.

N31J1RS, N351J1RS, N40J1RS and W95J1RS each pass 100 checks against the actual
source and core allocator. An independent absolute-index oracle checks
sample values, cumulative counts and metadata for small, mixed and
single-frame packets, plus flush/discontinuity and rate changes. RESBASE
uses the previous arithmetic-fixed source with exactly the same tests;
all four J1RB reports reproduce twelve failures, including 238000 rather
than 239991 frames. RSPROOF.JSN pins the reports and candidate hashes;
PATCHES/RSTREAM.DIF isolates this change. The plugin is staged on the four
private disks but is not yet integrated into BUILD/ASM or the installer.
These checks establish the resampler defect and fix, not the cause of every
observed A/V failure. Fresh output measurements remain necessary.

The combined candidate passes J2 on all four guests: cache, clock/condition,
audio and video reference comparisons, the 14-program suite, expanded GUI
and module preflight jobs all exit zero (J2PROOF.JSN). This validates content
and controls, separately from device timing. TESTS/GDISUB.C additionally
observes five real GDI pixel states over constant video: blank, left overlay,
blank, right overlay and blank. All three NT guests pass its 25 checks with
the prepared-surface candidate (J3GS reports, GSPROOF.JSN). This catches an
overlay left in the cached bitmap or a missing new overlay; Win95 execution
is pending because its DOS copy did not expose the test floppy.

W95J4 is a failed captured-output regression of the first stateful resampler:
all 80 scenes are observed, but AVCHK rejects 32 timing/duration/spacing
checks. QPACK's captured nine-tone check also fails after a late-PCR reset;
five short clips and the bounded AOUT tones pass. W95J4PIN.JSN independently
reads back the exact four candidate DLLs after clean shutdown. These failures
are retained; the source-level resampler pass does not override them.

SOURCES/RSDIV.C and BUILD/RSPREV preserve the division-per-sample implementation.
The next candidate replaces that inner-loop division with a quotient and
remainder step, retaining the same absolute sample sequence. RSDIV.EXE
compiles the preceding source for a measured comparison with the optimized
RESAMP.EXE. The reports include a coarse accumulated filter duration as a
diagnostic, not a performance pass threshold. W95J5's optimized resampler
passes all 100 checks and reproduces the expected stateless negative control.
Its real output acceptance remains pending.

On W95, the optimized and division-per-sample variants both pass 100 checks;
their accumulated filter durations are 46 and 129 ms respectively
(W95J5RS/RD.TXT). The next closed capture W95J6SN passes five short clips and
bounded continuous tones, but still fails the Opus packet-size capture.
All 80 A/V scenes are present with five failed checks, including two large
midstream video delays. This is not a release pass. A separate WTRACE plugin
records actual write/restart/cursor-query times to investigate these failures;
WTRACE.SH restores the uninstrumented working source after building it.

The first Win95 GDI subtitle harness run W95J3GS fails its fixed dark-pixel
threshold: the supplied driver maps the unchanged blue background to mean
intensity 85, versus 42 on NT. The report still records the correct sequence
of left/right white markers and background restoration. The corrected harness
first requires a rendered nonblack, nonwhite background, then requires that
same background after each overlay. SOURCES/GSSRC1.C and BUILD/GDISUB1 preserve
the initial harness and the failed report remains unchanged. No audio or
A/V timing threshold is changed by this harness correction.

W95J7GS passes the corrected GDI harness (25 checks), including restoration
of the measured background after both overlays. GS2PROOF.JSN pins it; the
same revised harness is staged for new NT runs.

The first diagnostic WTRACE capture W95J8 retains ten failed A/V checks and
a failed Opus capture. Its AAC restart call takes about 35 ms; the first
positive cursor reading implies an origin about 61 ms behind the scheduled
start. WTRACE2 records positive positions and periodic later readings.
The closed W95J9 Opus capture passes without resampling warnings. Its probe
estimates (cold 0 ms, warm 30 ms for the first file) differ from earlier runs;
an isolated diagnostic pass is not evidence that this variability is fixed.

The next WPROBE candidate queues at least 250 ms of silent calibration PCM
while still observing the first 40 ms of reported playback. A single 100 ms
probe can exhaust a driver's render-ahead ring (WINAUDIO uses about 70 ms)
during that interval. Each calibration header is limited to 32 KiB and the
queue is limited to sixteen headers. All calibration callbacks are excluded
from media-buffer accounting, and retained driver-owned buffers are retried
during output shutdown. PATCHES/WPROBE.DIF records the change; WPSTAGE.JSN
pins staging on all four cleanly stopped guests. This hypothesis requires
fresh output tests; no timing limits have been loosened.

JBPROOF.JSN now pins all four optimized-resampler and corrected GDI runs:
100 and 25 checks per guest respectively. The first WPROBE output capture
W95JASN passes the nine-tone Opus and short-clip checks. A/V still fails
21 checks, and the bounded continuous-tone check fails to recover the full
ordered sequence. The payload remains unaccepted. MMCORE provides a single
clock-selection comparison: it removes STARTFIX's automatic Win9x preference
for the performance counter, retaining the upstream multimedia default and
all condition/first-picture fixes. Other staged binaries remain unchanged.
MMSTAGE.JSN and PATCHES/MMCORE.DIF pin this comparison. No shared-start lead
increase or driver source change has been made.

The MMCORE comparison W95JC retains four A/V failures (all 80 scenes are
observed) and a failed Opus nine-tone capture. W95JD short clips and bounded
continuous tones pass against the same closed W95JCSN.WAV. Restoring the
multimedia default therefore does not solve the output failures. WTCLOCK is
a diagnostic-only WPROBE build: it samples the native cursor, VLC clock,
multimedia clock and performance counter together. Its source is separate
and the working uninstrumented source is restored after building.

NT3.1 JC/JD passes Opus packets, short clips and bounded continuous tones,
but the original A/V capture retains three failed checks. The first-picture
miss also occurs on NT3.1; two later pulse spacings are about 459 ms.
The WTCLOCK diagnostic measures both Windows clocks together with the native
waveOut cursor. Their difference stays within a few milliseconds while the
projected device origin moves by tens of milliseconds. QEMU 10.0.13's HDA
timer model drops its entire 8192-byte buffer on overrun (about 42.7 ms at
48 kHz stereo 16-bit), and independently slews its DMA timer to control buffer
fill. W95JGHT.LOG records an actual hda_audio_overrun during the A/V test.
This is a demonstrated emulation fault, not proof that every player timing
failure has the same cause. QEMU13.JSN pins matching upstream source snapshots;
Debian patch differences are not audited. The W95JE/JG closed captures retain
their failed checks. VM.SH now permits an explicit RP_VM_HDA_TIMER=off
comparison and optional preserved HDA event logs; its default remains on.

A separate LEADCORE candidate increases the common post-buffering preparation
interval from 150 to 500 ms. Decoder readiness precedes first GDI conversion;
the existing interval expires before first presentation on both NT3.1 and
Win95. This is a playback scheduling change, not a test tolerance change.
The candidate is built separately and has not been staged for the initial
HDA mode comparison, which retains the exact MMCORE/WTCLOCK payload.

The single-variable HDA callback-mode comparison W95JH retains 39 A/V
failures while observing all 80 scenes; Opus passes. This mode is not accepted
as a correction. W95JHVM.JSN records the actual QOM use-timer=false value
and exact QEMU command. WCTSC adds CPUID-guarded raw processor-counter
observations to the diagnostic plugin, preserving the uninstrumented source.
The next comparison restores QEMU's original timer mode and keeps MMCORE;
LEADCORE remains separate. No raw processor clock is selected for playback.

W95JJ's CPUID-guarded raw TSC trace identifies a second timing problem: the
Windows performance counter loses about 69 ms in the first fixture and
51 ms in the second relative to stable processor-counter intervals. The
native audio cursor follows the processor counter within its update jitter.
W95JJCL.JSN retains every observation and the scale-estimation method; it is
a relative-discontinuity diagnostic, not an independent absolute calibration.

TSCCORE adds RPCLOCK.H. Automatic selection is restricted to the exact
TCGTCGTCGTCG hypervisor signature, a CPUID-advertised TSC and one guest CPU.
It requires a consistent cluster of measured frequencies before selection,
keeps long-running conversion products bounded, and falls back to multimedia
timers when detection or calibration fails. Other hardware keeps the prior
default; merely having RDTSC is not treated as sufficient. Explicit clock
selection remains available. The candidate also includes LEADCORE's shared
500 ms preparation interval. TSCSTAGE.JSN pins the first W95 test payload,
which retains WCTSC diagnostics to verify the selected clock. Acceptance
and fallback-path validation are still pending.

TSCCORE's W95JK/JL capture passes Opus, short clips and bounded continuous
tones but retains two A/V failures: the initial AAC/H.264 scene and one
midstream pulse spacing. W95JKHT.LOG records one HDA timer-model overrun.
W95JKCL.JSN shows the player's clock following the processor counter while
the Windows performance counter loses time; no audio resampling correction
is logged. The uninstrumented candidate is staged on all four systems in
TSC2STG.JSN. JP passes the content matrix, including a TSC-less 486 NT3.1
run (CPUTEST reports scalar FFmpeg flags). NT4's Welcome window initially
clips the GUI test; the unchanged JR repeat passes after the window is
closed. JPPROOF.JSN and N40JRFIX.JSN preserve both outcomes. AVCLOCK is a
separate first-timestamp diagnostic, built but not yet staged.

The uninstrumented TSCCORE callback-mode capture W95JM/JN passes nine
Opus tones, five short clips and the isolated continuous-tone sequence.
Its original A/V check still fails three scenes, including initial AAC/H.264
presentation. NT3.51 JM/JN passes short clips but fails A/V (32 findings),
Opus and continuous tones; repeated resampling and inserted-silence messages
remain in its logs. These failures are preserved, not accepted by association
with the Win95 passes. AVCLOCK and WCTSC are staged only as diagnostics on
Win95 and NT3.51; the first new Win95 trace is JS.

W95JS directly identifies the first MP4 timestamp error: AVFIRST records
AAC at 67273 ms and the first H.264 picture at 67473 ms, with raw video
PTS 200 ms. The first picture is prepared at 66912 ms, well before its
incorrect deadline. MKV audio and video both map to 94138 ms. The native
MP4 demuxer applies ELST to DTS and clamps negative values before adding
CTTS; this incorrectly shifts initial reordered pictures. MP4TIME computes
composition time before applying ELST, while preserving existing decoder
DTS handling. MP4STAGE.JSN pins the baseline, candidate and diagnostic.
W95JT is the first single-module runtime comparison.

NT3.51 JS confirms a separate reference-clock problem. Its TSC-selected
VLC clock follows QPC, but both advance more slowly than multimedia and
the native sound cursor. QPCBIAS.JSN records long-interval comparisons,
including multimedia tick loss under load. TSCGUARD increases the idle
calibration span to about 700 ms and cross-checks its selected rate against
multimedia time (5 ms quantization allowance plus 0.5%). A mismatch falls
back to the existing multimedia clock; this is not yet an accepted output
fix. PATCHES/TSCGUARD.DIF preserves the incremental change.

W95JT passes the unchanged 80-scene output check. AVFIRST confirms that
the corrected native MP4 first video PTS is zero and maps to exactly the
first audio deadline (118383 ms); preparation finishes before that deadline,
and visible completion is 9 ms later. MP4PROOF.JSN pins this single-module
comparison, the independent host packet timestamps and unchanged diagnostic
core/audio binaries. NT3.51 JT/JU next tests TSCGUARD plus MP4TIME, retaining
WCTSC solely to observe the selected clock. NT31, NT4 and W95 are staged with
the uninstrumented combined candidate after clean shutdown.

TSCGUARD's NT3.51 JT/JU run passes all 80 A/V scenes, short clips and
bounded continuous tones. TGFALL.JSN records that all 172 diagnostic clock
observations use multimedia time, confirming rejection of the biased QPC
calibration. The first 2.5 ms Opus triad still fails its last-tone duration
check. N351JTPF.JSN shows the tone is present but its pitch sweeps from
315 to about 310.5 Hz after drift correction starts; it is not a truncated
file. The core adjusts resampling by 2 Hz per packet, so 2.5 ms packets
change rate eight times faster than ordinary 20 ms packets. SYNCRATE
preserves the 2 Hz per 20 ms slope using media duration, retains fractional
intervals and applies individual steps so return-to-zero cannot overshoot.
Its new candidate is separate from the accepted ASM payload.

SYNCRATE JV/JW passes NT3.51 A/V, short clips and continuous tones, but
Opus still fails with further clock-driven correction. The host packet-rate
property remains independently verified; that improvement does not repair
the timing reference. MMTSC instead calibrates the guarded TCG counter
directly against about 1.75 seconds of idle multimedia time, rejecting
discontinuous 250 ms subintervals. It avoids inheriting the HAL's biased
QPC frequency. TCGUNIT2 exercises the actual calibration/conversion code
with quantization, wrap, missing APIs, unavailable TSC and counter faults.
MMTSSTG.JSN pins the separate candidate and the earlier failed test case.

W95JV/JW with uninstrumented TSCGUARD passes all audio-only checks but
retains two A/V bounds failures. The optional host-only WAVSINK helper
removes stdio buffering from exactly the selected WAV stream, without
changing samples, device buffering or guest clocks. AVRUN verifies its
activation proof and process mapping before recording a zero stdio bound;
otherwise the original 8192-byte allowance remains. The 150 ms timing
criterion is unchanged. AVCKREG.JSN reproduces the previous buffered JS
report byte for byte. W95K0 restores the old MP4 plugin as a negative
control: it still fails the first scene, plus three later scheduling
outliers. This measurement change is not a player fix or a passing test.

MMTSC's K1/K2 captures still require investigation: NT3.51 passes A/V,
Opus and short clips but fails continuous tones; W95 passes Opus and
short clips but fails one A/V scene and continuous tones. NT3.51's clock
trace uses multimedia fallback, despite the simulated calibration passing.
CLKPROBE reads the actual idle intervals: immediately after timeBeginPeriod,
the first nominal 250 ms interval contains only about 236 ms of processor
ticks. The following two calibrations select approximately 2.596 GHz.
CLKPHASE.JSN pins the native report, recovered guest diagnostic binary and
old calibration source. MMTSC2 waits one interval before recording its first
endpoint. TCGUNIT4 passes 32 behavioral checks; TCGOLD2 fails the simulated
initial-phase discontinuity with the preceding production code. MM2STAGE.JSN
pins the same uninstrumented candidate on all four private disks for the
next content/output matrix. No candidate has been integrated into ASM.

MMTSC2's native probe selects the processor clock in all three idle trials
on each guest (N31K4CP, N351K4CP, W95K4CP and N40K8CP.TXT). These diagnostic
runs do not establish playback acceptance. The K5 content matrix passes
all seven jobs on NT3.51 and Win95. NT3.1 passes six jobs; AFMTEST's first
MP3 case times out after collecting 79488 of 96000 expected samples, while
the other 23 cases pass. Preserve that failure for isolated investigation.
NT4 also passes all seven content jobs (K5PROOF.JSN). N40K6BT.WAV is only an interrupted
boot/automation capture, not an accepted audio result. A later NT4 run
reaches its desktop and completes CLKPROBE; its long report request exposed
UART input loss, corrected by pacing host commands in VMRUN.PY.

K2PITCH.JSN examines the earlier failed bounded AOUT captures. All three
tones are present, but clock correction shifts their frequencies (including
NT3.51's final nominal 315 Hz tone toward 310 Hz). The strict analyzer and
its acceptance thresholds remain unchanged.

K7/K9 closes the sequential MMTSC2 output matrix using uninstrumented player
binaries. NT3.1, NT4 and Win95 pass all four independent checks: 80-scene
A/V timing, nine Opus tones, five short clips and bounded continuous tones.
NT3.51 passes all three audio-only checks but fails HEVC scene 11: audio
precedes the visible transition by 338–406 ms, outside the unchanged 150 ms
limit. K7PROOF.JSN pins all four closed captures and sixteen reports. The
candidate remains unaccepted; the isolated NT3.1 AFM repeat passes all 24
cases, while its earlier concurrent K5 MP3 timeout remains in K5PROOF.JSN.
