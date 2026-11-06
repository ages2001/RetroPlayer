# Native Xiph modules

XPREP.SH verifies SOURCES/XIPH.JSN, prepares the pinned source trees and
applies VORBIS.DIF and FLAC.DIF. XIPH.SH builds static i386 libraries with
the shared legacy runtimes. XMOD.SH links VLC's native Ogg, Vorbis, Opus and
FLAC modules and rebuilds the avformat adapter, then audits the resulting
254-plugin package. These scripts require the existing ASMBLD core and
ASMLIB dependency profile. They do not constitute a clean-machine bootstrap.

The pinned versions are libogg 1.3.6, libvorbis 1.3.7, FLAC 1.4.2 and
Opus 1.6.1. The first three archives match VLC's contrib SHA-512 checksums.
The Opus source archive is from the pinned v1.6.1 Git commit; XPREP.SH writes
its package version and runs autoreconf without downloading optional neural
models. Source archives and redistribution licenses are included.

    sh XIPH.SH
    sh TSCORE.SH
    sh XMOD.SH
    sh AFMTEST.SH

The new libraries currently use scalar i386 code. Additional instruction-set
dispatch is unfinished. This does not change the existing FFmpeg assembly
profile or imply hardware acceleration.

RPCRT supplies the newly needed _wchmod, _wutime64 and _filelengthi64 APIs.
CRTEST covers read-only attributes, modification timestamps beyond 2038,
current-time updates, invalid paths/descriptors and unchanged file position.
NT uses wide Windows paths; Windows 95 uses the existing lossless ANSI
conversion. These are real file operations, not success-only stubs.

The audio matrix now requires the native module identities and exact PCM
sample counts for its Xiph cases. XMEDIA.PY adds short files, 2.5/60 ms Opus
packets and chained streams. The native-path trimming changes and expanded
matrix passed on NT3.1, NT3.51, NT4 and Win95: N31X5AF, N351X5AF, N40X5AF
and W95X5AF.TXT in RESULTS. Older AFM4 results cover the previous FFmpeg path.
