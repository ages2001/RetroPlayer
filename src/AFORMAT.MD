# Audio format checks

AFMTEST.EXE exercises MP3, FLAC, ALAC, Vorbis, Opus in Ogg and Matroska,
AC-3, E-AC-3, WMA1, WMA2, WavPack, TTA, AIFF PCM and mu-law WAV through
the ported LibVLC. These are 14 cases covering 13 audio coding formats.
Ten additional Xiph boundary cases bring the matrix to 24 files. The original
fixtures are generated from an owned two-second chirp, another tone and
deterministic noise. No third-party music is included.

The host FFmpeg independently decodes each file to mono 48 kHz signed
16-bit PCM. The guest compares its full decoded waveform with that reference.
The check allows up to 4096 samples of codec/container delay, requires at
least 95 percent reference coverage and a signal/error ratio of at least
40 dB. Silent output, a missing decoder and a short matching prefix fail.
The original formats allow codec padding. Native Xiph cases additionally
require exact sample counts, zero alignment offset and native decoder/demux
identities. These checks establish the tested files' trimming, not seamless
playlist transitions or audio device output. The HDA and SB tests use a separate real waveOut path.

BUILD/ASM/AFREFS.JSN pins the encoded files and reference samples. AFMTEST.SH
reuses and verifies those fixtures; set RP_AF_REGEN=1 to regenerate them.
RESULTS/AFVERIFY.JSN records identical fixture hashes in all four guest disks
and the package. The reference source is SCRIPTS/AFMEDIA.PY.

The first matrix found that VLC's Xiph-laced Opus headers were passed to
FFmpeg as raw codec headers, preventing decoding. The first fix exposed a
second bug: discarding an initial negative-timestamp Opus packet lost decoder
predictor state. The final adapter extracts OpusHead and decodes the pre-skip
packet at stream start. It leaves malformed headers for the codec to reject.
The changes are in PATCHES/LEGACY.DIF.

Early failed reports are retained. N31AFM4, N351AFM4, N40AFM4 and W95AFM4 report all
14 cases passing after the fix. The two Opus cases then have zero alignment
offset and at most one signed-16-bit sample unit of error. Trailing packet
padding is permitted by this test and is not claimed to be gapless playback.

## Native Xiph regression

N31X5AF, N351X5AF, N40X5AF and W95X5AF.TXT pass all 24 cases. XVERIFY.JSN
records identical binaries and fixture hashes on the four offline guest
disks. XSUM.JSN records report hashes. XIPH.MD describes the native libraries
and the added CRT APIs.

The Ogg adapter preserves header pre-skip, retains priming packets, computes
timestamps in integer samples and carries single-page EOS trimming. Chained
streams are queued until their ES and timestamps are ready. The avformat
adapter carries Opus start/end padding to the native decoder. Vorbis honors
the final duration, and Opus applies start and end trim independently.

Reference generation also checks boundary behavior. The host FFmpeg produced
wrong lengths for short Vorbis files, so VORREF.PY uses host libvorbisfile
with an explicit seek to sample zero and checks the granule-derived total.
For chained Opus, XREFS.PY opens a fresh host FFmpeg decoder per logical
stream. The whole-chain FFmpeg result differed after the boundary; separate decoding
matched the guest within one PCM unit.
XREFCHK.JSN retains both reference hashes and the measured comparison; the
earlier failed reports remain available. These changes correct reference
construction rather than relaxing the content or length checks.

The content regression uses one second of file buffering. X6/X7 NT3.1
runs with the former 100 ms setting dropped late Vorbis or Matroska Opus
blocks (the logs identify the drops); these reports remain failed. A longer
startup buffer is appropriate for the 32 MiB Pentium test VM. It does not
relax sample counts, alignment, coverage or error limits, and does not replace
the separate waveOut/captured-device timing tests. VMTEST.PY now runs guests
sequentially by default; --parallel is an explicit diagnostic stress mode.

The one-second cache also exposed a real short-chain defect (N31X8AF.TXT):
both 250 ms links fit inside the initial buffer, and deleting the first
decoder discarded its queued audio. The core now releases initial buffering
and drains a departing active decoder before deletion; stopped/paused inputs
do not start playback as a side effect. Both chain fixtures must still match
all 24000 independently decoded samples. The X9 audio matrix and 14-program integrated suite pass on all four guests.
