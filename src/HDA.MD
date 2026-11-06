# HDA output integration

The test VMs use Intel HDA with the QEMU duplex codec. HDA remains the primary configuration. The optional `RP_VM_AUDIO=sb16`
comparison uses ISA address 0x220, IRQ 5, DMA 1/5 and writes a separate
`RESULTS/<guest>SB.WAV` capture. `RP_VM_AUDIO=hda sh VM.SH NT31` enables the device and writes
`RESULTS/NT31SND.WAV`; substitute NT351, NT40 or W95 for the other guests.
The WAV header is complete only after QEMU exits. Save each capture before
starting another VM with the same guest name. Input capture is not tested.

## Drivers

- Windows NT 3.1/3.51/4: AUDIONT, revision
  ff36dc8e14f918c62f73761a774a5fc9688583bf. Run DRIVERS/NT/SETUP.EXE in
  the guest, complete installation and restart. ANTCORE.SH builds this pinned
  native NT driver and its tools; it does not build a WDM or x64 variant.
- Windows 95: WINAUDIO, revision
  985e5e57a8ccdfecce4ea828b3adf5fcda063111. Copy DRIVERS/WIN9X to
  C:\WINAUDIO\WIN9X. In New Hardware Found, choose Have Disk and this
  directory, then WINAUDIO High Definition Audio Bus Controller. Complete
  codec discovery and restart. This procedure was used on the supplied RTM VM.

SOURCES/AUDIONT.JSN and WINAUD.JSN record revisions and file hashes. The
corresponding source archives and GPLv3 licenses are included. HDAPACK.PY
verifies the archives and every staged file. Drivers remain separate from
the player installation. Player paths remain C:\RETROPLA on NT3.x and
C:\Program Files\RetroPlayer on NT4/9x.

The WINAUDIO core requires Pentium/TSC; this HDA configuration does not
establish sound support on an actual 386. Hardware supported by the upstream
drivers is not automatically certified by the emulated HDA tests.

## Output checks

AOUTTEST.EXE uses LibVLC's waveOut module and writes AOUTTEST.TXT and
AOUTLOG.TXT. Its generated PCM file contains 441, 735 and 315 Hz triangle
waves separated by silence. A 20-second silent preroll permits initial VLC
clock correction to settle. This is explicit: the first short Windows 95
capture showed a temporary pitch correction, and the longer log records
resampling stopping. Exact pitch is checked after that initial interval.

SCRIPTS/SNDCHK.PY checks the closed host capture independently: ordered
frequencies, duration, silence intervals, clipping, dropouts and identical
mono content in both output channels. A guest Ended state alone is not a pass.
RAWTEST.EXE isolates the driver with PCM buffers below 64 KiB. Use --raw
when analysing that capture. A preliminary single 441000-byte buffer repeated
data on the 16-bit Windows 95 driver path; that oversized-buffer case is not
claimed supported. The final direct-driver test uses twenty 22050-byte buffers.

The waveOut module initializes its software volume state before a device is
opened. When a legacy mapper advertises volume but rejects querying the stream
(observed on NT3.1), the module uses software gain and does not poll unsupported
hardware volume. These changes are included in PATCHES/LEGACY.DIF.

Physical audio hardware, microphone input, all sample formats and A/V sync
remain outside the evidence from these tests.

## Earlier Sound Blaster checkpoint

After the audio-format fixes, the supplied Windows 95 RTM installation was
tested with QEMU SB16 at 0x220, IRQ 5, DMA 1/5. Its native Creative Labs
Sound Blaster 16 or AWE-32 driver was installed using Add New Hardware and
the existing C:\95SET installation files. Automatic detection missed the
card, so the model was selected manually. The native OS driver is not
redistributed in the RetroPlayer package.

W95SBAO1.TXT reports one device, `SB16 Wave Out [220]`, and 18 checks passed
with no waveOut errors. W95SB.JSN independently verifies 441, 735 and 315 Hz
tones in the closed W95SB.WAV capture, including timing, silence, channels,
clipping and dropouts. The log records initial clock correction stopping.
This earlier SB comparison was performed on Win95; the NT results use AUDIONT
with HDA. VM.SH defaults to HDA and keeps the optional SB capture separate.

## Later Xiph integration regression

X5 guest waveOut checks passed on all four guests, but independent captures
N351X5HD.JSN and N40X5HD.JSN failed the strict frequency check. N31X5HD.JSN
and W95X5HD.JSN passed. The failed captures and logs are retained. These
failures exposed startup clock correction continuing into the test tones.
The waveOut adapter now waits for the first buffer's scheduled timestamp
after opening/resetting the device; the X9 and XA captures below validate
this change in isolated runs. The earlier successful checkpoint is not evidence that this
regression passed. A fresh SB test is required before packaging this change.

## X9 core regression

The changed core passes all four guest waveOut checks. Independent captures
N31X9HD, N351X9HD and N40X9HD.JSN pass. W95X9HD.JSN fails: a concurrently
running NT3.1 installation produced late/early correction that continued
into the tones. The failed capture and log are retained. The isolated Win95
repeat W95XAHD.JSN passes, using the same core and strict analyzer. Host load must be controlled for device
timing measurements; ordinary decoder tests do not establish sound timing.


## Queued startup regression

The current waveOut candidate pauses the device and queues 100 ms before
starting at the shared presentation date. This avoids exhausting the first
short AAC/Opus packet while the legacy output path starts. Clips shorter than
that threshold explicitly start during drain. The closed NT3.1 capture
N31ABSN.WAV passes independent A/V (N31ABCK), short-clip durations (N31Y2QD)
and continuous-tone checks (N31Y2HD). Other HDA guests and fresh SB remain
pending for this candidate; X9 audio is earlier evidence.

## Current timing investigation

QEMU 10.0.13's HDA timer model can drop its entire 8192-byte output buffer
on overrun. W95JGHT.LOG records this event during an audiovisual test.
The same run family also exposes lost legacy Windows timer ticks: W95JJCL.JSN
compares the Windows performance counter, processor counter and native audio
cursor. The processor-counter scale is estimated from stable intervals;
this is a relative timing diagnosis, not physical-device certification.

VM.SH keeps the default HDA timer model enabled. An explicit comparison uses
RP_VM_HDA_TIMER=off; RP_VM_HDA_TRACE=<8.3.LOG> saves buffer-overrun and timer
adjustment events without overwriting an earlier log. It also records QEMU
shutdown-request reasons to distinguish guest exits from player failures.
The callback-mode
comparison alone fails W95JH, so this option is not documented as a fix.
TSCCORE separately gates a calibrated counter on the exact single-CPU TCG
environment and retains the prior default for other hardware. Its initial
W95JK/JL audio-only captures pass; two A/V checks still fail. The original
WINAUDIO and AUDIONT driver sources have not been changed.

## Optional precise capture cursor

`sh WAVSINK.SH <report-tag>` builds and checks a host-only observer.
`RP_VM_WAV_SYNC=1 RP_VM_WAV_PROOF=<fresh-8.3.TXT> sh VM.SH <guest>`
removes stdio buffering only from that guest's WAV capture. AVRUN checks
the live QEMU mapping and activation proof before omitting the previous
8192-byte stdio uncertainty; the 150 ms A/V limit and device scheduling
allowance are unchanged. Without verified activation the original bound
still applies. WAVCK2.JSN verifies stream isolation and refusal to overwrite
activation evidence. AVCKREG.JSN reproduces an older buffered report exactly.
W95K0 retains the old MP4 plugin as a negative control and still fails its
first scene. The observer is not part of the Windows player or its drivers.
Stop any VM using the helper before rebuilding it.
