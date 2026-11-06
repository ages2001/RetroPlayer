# Speex candidate modules

Speex 1.2.1 and SpeexDSP 1.2.1 match the pinned VLC contrib versions and
SHA512 values. SOURCES/SPEEX.JSN records the mirrors and archive hashes;
SPPREP.SH verifies/extracts them, SPEEX.SH builds portable i386 libraries,
and SPMOD.SH builds Ogg and both Speex candidates in BUILD/SPMOD. SSE and NEON are
disabled in these dependency builds. After all four guest regressions passed,
SPMERGE.PY integrated the three tested DLLs into BUILD/ASM (256 modules).
RESULTS/SPINTEG.JSN records the hashes and evidence. The installer payload
still contains the preceding 254-module checkpoint.

SPMEDIA.PY generates owned narrowband, wideband, ultra-wideband, stereo,
short, multi-frame-packet and chained Ogg fixtures. Host FFmpeg with its
libspeex decoder provides native-rate/channel PCM references. SPTEST.EXE
requires the native Ogg and Speex modules, an exact sample count, nonsilent
output and at least 40 dB signal-to-error ratio across the full stream.
It does not realign samples or discard leading/trailing audio to obtain a pass.

SPRES.EXE exercises the actual VLC Speex resampler with six conversion ratios
and whole versus irregularly chunked input. SROLD.EXE builds the same test
against the unchanged upstream source. The candidate rejects zero rates/frame sizes before division. The initial
duration-cast change was redundant because CLOCK_FREQ already widens the
arithmetic; it has been removed. NT3.1 passes 1961 current-source checks and
1956 upstream checks for valid-rate duration and chunk continuity. S1
incorrectly expected the upstream comparison to fail; its actual report is
PASS and the runner now expects EXIT 0. This is not an arithmetic bug fix. The Speex resampler keeps upstream score
zero; it is not selected as the default resampler by this change.

N31SPSTG.JSN pins the initial candidate/test files copied to the stopped
private NT3.1 disk. The first native decode run (N31S1SP.TXT) loses one complete initial packet
in all nine cases. Comparing the remaining samples to the corresponding
reference offset gives less than one squared sample unit of mean error.
The Ogg demuxer marked the entire first packet as preroll when its derived
start was negative. The new candidate retains that Speex packet, just as the
Opus path already retains priming packets. Ten cases, including a 2.3-second
multipage stream, pass on NT3.1 in N31S2SP.TXT.

SOURCES/SPFIX1.TGZ preserves the initial nine fixture/reference files;
RESULTS/N31S1EX.EXE preserves that run's test binary. The second candidate and
its rebuilt test are not integrated into ASM or the installer.

The subsequent N31S3 audio regression failed five Vorbis cases: SPMOD.SH had
rebuilt the shared Ogg module without the Vorbis flags used by XMOD.SH. The
script now rebuilds both Ogg translation units with HAVE_LIBVORBIS and links
the pinned static Vorbis/Ogg libraries. N31S4ST.JSN pins this corrected build.
NT3.1 S4 passes all ten native Speex cases, 1961 resampler checks and 1956
unchanged-upstream comparison checks. N31S5 passes the existing AFMTEST audio
regression. S4STAGE.JSN records deployment to the other three private disks;
All four guests pass S4 (all ten cases and both resampler suites), and S5
passes existing audio regressions. NT3.51/NT4/Win95 S5 also pass video and UI;
NT3.1 S6 supplies the separate 82-check video regression. SPMERGE.PY checks
all four Speex results and audio/video regressions,
verifies report/candidate hashes and backs up only the affected baseline
modules before integration. It leaves the installer and audio-output/core
candidates untouched.
