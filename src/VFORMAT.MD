# Video and container regression matrix

VFMTEST.EXE uses automatic VLC demux selection and checks twelve distinct
decoded frames per file. It covers MPEG-1/2 program streams, MPEG-4 and
MS MPEG-4 AVI, WMV1/2 ASF, Theora Ogg, VP8 WebM, FFV1 Matroska, FFVHUFF AVI,
Motion JPEG AVI, JPEG 2000 Matroska, FLV1, Cinepak AVI, H.264 transport streams
and H.264 MOV. AAC remains a final audio regression in this test executable.

Every expected pixel in the visible 64x48 I420 frame must match the independent
host reference within one 8-bit unit. The bound accounts for measured integer
rounding differences between host and target JPEG 2000/Cinepak decoders and
color conversion. Motion JPEG's full-range output is explicitly converted to
limited-range I420 for comparison with the negotiated LibVLC output. Repeated
display callbacks are allowed only for the previous validated frame. Missing,
reordered, corrupt or solid-gray frames fail.

SCRIPTS/VFMEDIA.PY generates the owned fixtures. BUILD/ASM/VFREFS.JSN pins the
encoded files, decoded references and frame hashes. VFMTEST.SH verifies and
reuses them unless RP_VF_REGEN=1 is set. VFMTEST is compiled from MEDTEST.C
with a separate profile; existing strict-hash media tests retain their checks.

The first run found a lost final ASF frame and incorrect MPEG-TS demux
selection. The ASF fix flushes an assembled frame even when an index or
another ASF header follows the data object. TSCORE.SH builds libdvbpsi 1.3.3
with VLC's eight contrib patches and then the native TS module. The source
archive matches VLC's SHA-512 checksum; SOURCES/DVBPSI.JSN and DVBPSI.DIF
record the dependency. The final X9 matrix passes on NT3.1, NT3.51, NT4 and Win95.

The native TS demuxer also drains an unbounded final video PES at file EOF;
explicitly sized incomplete PES packets remain rejected. Initial X6 reports
showed eleven of twelve frames; W95X7 and N31X7 show all twelve after the fix.

N31X8 exposed a separate first-picture scheduling problem: its diagnostic
dump exactly matches reference frame 1, so frame 0 was skipped rather than
corrupted. The display loop prepared two pictures and replaced the first
before showing it when the second was already due, even with frame dropping
disabled. The core now preserves that first picture in no-drop mode.
The X9 audio/video matrices and 14-program integrated suite pass on all four
guests with these core changes. X9SUM.JSN pins the core and reports.
