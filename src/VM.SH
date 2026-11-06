#!/bin/sh
# Start an existing test overlay. Originals are backing files, never writable.
set -eu
cd "$(dirname "$0")"
target=${1:?Usage: sh VM.SH NT31 or W95}
case "$target" in
  NT31) memory=32; base=NT31TEST.vhd ;;
  W95) memory=64; base=W95TEST.VHD ;;
  NT40) memory=64; base=NT40TEST.VHD ;;
  NT351) memory=64; base=NT351.VHD ;;
  W2K) memory=128; base=win2000test.vhd ;;
  XP64) memory=256; base=xp64test.vhd ;;
  *) echo 'Unknown target' >&2; exit 2 ;;
esac
# Windows 2000 and XP x64 boot from an AHCI controller (installed with the
# AHCINT miniport); the older guests use the emulated IDE controller.
case "$target" in W2K|XP64) storage=ahci ;; *) storage=ide ;; esac
root=/workspace/TOOLS/SYSROOT
memory=${RP_VM_MEMORY:-$memory}
case "$memory" in 32|64|128|256|512) ;; *) echo 'Unsupported test memory size' >&2; exit 2 ;; esac
cpu=${RP_VM_CPU:-pentium}
case "$cpu" in pentium|pentium2|pentium3|486) ;; *) echo 'Unsupported test CPU' >&2; exit 2 ;; esac
export LD_LIBRARY_PATH="$root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
disk="TESTDISK/${target}RUN.QC2"
if [ ! -f "TESTDISK/${target}RUN.QC2" ]; then
  "$root/usr/bin/qemu-img" create -f qcow2 -F vpc \
    -b "$PWD/TESTDISK/$base" "TESTDISK/${target}RUN.QC2"
fi
if [ -f "TESTDISK/${target}EDIT.QC2" ]; then disk="TESTDISK/${target}EDIT.QC2"; fi
if [ "$target" = NT351 ] && [ -f TESTDISK/N351EDIT.QC2 ]; then disk=TESTDISK/N351EDIT.QC2; fi
# QEMU creates its monitor socket before obtaining the disk lock. A second
# launch could otherwise unlink a live VM's monitor and make it unreachable.
python3 - "$PWD/$disk" <<'PY'
import sys
from pathlib import Path
disk = sys.argv[1].encode()
for entry in Path('/proc').iterdir():
    if not entry.name.isdigit(): continue
    try: args = (entry/'cmdline').read_bytes().split(b'\0')
    except (OSError, PermissionError): continue
    if args and b'qemu-system' in args[0] and any(b'file='+disk+b',' in a for a in args):
        raise SystemExit('Test disk is already in use; keep the existing monitor.')
PY
set --
if [ -f "TESTDISK/$target.FLP" ]; then
  set -- -drive "file=$PWD/TESTDISK/$target.FLP,format=raw,if=floppy" -boot c
fi
case "${RP_VM_AUDIO:-hda}" in
  none) ;;
  hda)
    # Private guest output capture. This exercises the emulated device and
    # installed guest driver; it is not a physical sound-card certification.
    # Keep the default model reproducible. The callback mode is an explicit
    # diagnostic comparison for QEMU HDA timer buffer drops and rate slewing.
    hda_timer=${RP_VM_HDA_TIMER:-on}
    case "$hda_timer" in on|off) ;; *) echo 'HDA timer must be on or off' >&2; exit 2 ;; esac
    set -- "$@" -audiodev "wav,id=rpaudio,path=$PWD/RESULTS/${target}SND.WAV" \
      -device intel-hda,id=rphda \
      -device "hda-duplex,id=rpcodec,bus=rphda.0,audiodev=rpaudio,use-timer=$hda_timer"
    if [ -n "${RP_VM_HDA_TRACE:-}" ]; then
      python3 - "$RP_VM_HDA_TRACE" <<'PY'
import re, sys
from pathlib import Path
name = sys.argv[1]
if not re.fullmatch(r'[A-Z0-9]{1,8}\.LOG', name):
    raise SystemExit('HDA trace must have an uppercase DOS 8.3 LOG name')
if (Path('RESULTS')/name).exists():
    raise SystemExit('HDA trace already exists; preserve the earlier evidence')
PY
      set -- "$@" -D "$PWD/RESULTS/$RP_VM_HDA_TRACE" \
        -trace enable=qemu_system_shutdown_request \
        -trace enable=hda_audio_overrun -trace enable=hda_audio_adjust
    fi ;;
  sb16)
    # Optional comparison requested by the user. Keep its capture separate.
    set -- "$@" -audiodev "wav,id=rpaudio,path=$PWD/RESULTS/${target}SB.WAV" \
      -device sb16,audiodev=rpaudio,irq=5,dma=1,dma16=5,iobase=0x220 ;;
  *) echo 'Unsupported audio test device' >&2; exit 2 ;;
esac
# Optional host observation helper. Require a fresh activation proof; the
# analyzer retains its original stdio allowance unless the running process,
# mapped helper and proof all agree. Guest/device buffering is unaffected.
case "${RP_VM_WAV_SYNC:-0}" in
  0) ;;
  1)
    case "${RP_VM_AUDIO:-hda}" in
      hda) wav_capture="$PWD/RESULTS/${target}SND.WAV" ;;
      sb16) wav_capture="$PWD/RESULTS/${target}SB.WAV" ;;
      *) echo 'Synchronous WAV observation requires an output device' >&2; exit 2 ;;
    esac
    test -f BUILD/WAVSINK/WAVSINK.SO
    python3 - "${RP_VM_WAV_PROOF:-}" <<'PY'
from pathlib import Path
import re,sys
name=sys.argv[1]
if not re.fullmatch(r'[A-Z0-9]{1,8}\.TXT',name):
    raise SystemExit('Set RP_VM_WAV_PROOF to a fresh DOS 8.3 TXT name')
if (Path('RESULTS')/name).exists():
    raise SystemExit('Preserve the earlier WAV activation proof')
PY
    export RP_WAV_PATH="$wav_capture" RP_WAV_PROOF="$PWD/RESULTS/$RP_VM_WAV_PROOF"
    export LD_PRELOAD="$PWD/BUILD/WAVSINK/WAVSINK.SO${LD_PRELOAD:+:$LD_PRELOAD}"
    ;;
  *) echo 'RP_VM_WAV_SYNC must be 0 or 1' >&2; exit 2 ;;
esac
if [ "$storage" = ahci ]; then
  set -- "$@" -device ich9-ahci,id=ahci \
    -drive "file=$PWD/$disk,format=qcow2,if=none,id=rpos" \
    -device ide-hd,drive=rpos,bus=ahci.0,bootindex=0
else
  set -- "$@" -drive "file=$PWD/$disk,format=qcow2,if=ide"
fi
exec flock -n "/tmp/RETRO-$target.LCK" "$root/usr/bin/qemu-system-i386" \
  -L "$root/usr/share/qemu" -bios "$root/usr/share/seabios/bios-256k.bin" \
  -machine pc,accel=tcg -cpu "$cpu" -m "$memory" \
  -vga none -device "cirrus-vga,romfile=$root/usr/share/seabios/vgabios-cirrus.bin" \
  -nic none -display none -qmp "unix:/tmp/${target}.SOC,server=on,wait=off" \
  -serial "unix:/tmp/${target}.SER,server=on,wait=off" \
  -no-reboot "$@"
