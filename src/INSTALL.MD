# Win32 development installer

Build with `sh INSTALL.SH`, then `python3 SCRIPTS/INSPACK.PY`.
BUILD/SETUP contains INSTALL.EXE, its manifest and PAYLOAD. Extract the whole
folder and run INSTALL.EXE. This remains an incomplete development build;
the installer does not certify full format coverage or add Win16 support.

Setup selects C:\RETROPLA on NT 3.xx and C:\Program Files\RetroPlayer on
NT4 and Windows 95/98/ME. Win32s is explicitly rejected. Project files and
payload directory components use DOS 8.3 names. NT file operations use wide
paths; Win9x uses lossless conversion to the system code page.

Every source is checked against its manifest before changes begin. Files
are copied and checked in a temporary directory on the destination volume.
Existing application files are moved into a temporary backup before the new
files are moved into place. An error restores files in reverse order. If
restoration itself fails, the backup directory is retained for recovery.
This handles ordinary file-operation errors; it is not a transaction across
power loss. A successful update requires free space for the staged copy.
CRC32 checks accidental damage, not publisher authenticity.

Setup installs the player, plugins, bundled font and licenses. It does not
change the registry, associations, sound drivers or unrelated user files.
The enclosing development archive contains corresponding source bases,
patches and build scripts. It is required alongside the payload for source
and test evidence; the installer is not a substitute for those files.

INSTEST.EXE uses private temporary directories to test corrupt sources,
invalid names, duplicate entries, a locked destination, successful updates
and rollback after a previous destination has already been replaced. It
also checks the required default paths. The original harness passed 86 checks
on NT3.1, NT4 and Win95. NT3.51's copied installation had a nonexistent TEMP
path, so the harness now creates a unique private directory beside its report;
that updated harness passes on NT3.1 and NT3.51; NT4/Win95 reruns are pending.

The GUI installer completed on NT3.1 and NT3.51 at C:\RETROPLA (N31INST.PPM and
N351INST.PPM). N31INH1.JSN and N351INH1.JSN independently compare all 289 installed
files with the X9 payload: every SHA256 matches. NT4 and Win95 GUI installation
remain pending. Later engine changes require an updated payload and comparison.
SCRIPTS/INSVM.PY stages the complete installer only on stopped disposable
test disks, using the same original-image protections as VMIO.PY.


The current queued-audio payload has also completed GUI installation on
NT3.1 and NT3.51 (N31I2OK.PPM, N351I2OK.PPM). N31INH2.JSN verifies all 289
installed payload files against the current manifest, with no mismatches.
N351INH2.JSN independently verifies all 289 NT3.51 payload files as well.
The corrected installer harness passes 86 checks on NT3.51 in N351Y2IN.TXT.
NT4 and Win95 GUI installation and the remaining current regressions are pending.

The current payload also completes GUI installation on NT4 and Win95
(N40I2OK.PPM, W95I2OK.PPM). N40INH2.JSN verifies all 289 NT4 file hashes.
Win95's independent disk comparison awaits clean shutdown.

W95INH2.JSN verifies all 289 installed Win95 payload hashes after clean
shutdown. Thus GUI installation of the queued-start payload and its on-disk
contents are checked on all four guests. Separate playback candidates staged
subsequently are not covered by that installer manifest.
