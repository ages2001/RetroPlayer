# 16-bit build toolchain (Open Watcom v2)

The 16-bit player (RETRO16.EXE) targets real-mode-capable Windows 3.0, so it
needs a compiler that emits 16-bit NE executables with 8086 code. The GNU
MinGW cross toolchain used for the 32-bit player only emits 32-bit PE, so a
separate 16-bit toolchain is required.

RetroPlayer uses **Open Watcom v2** (wcc, wrc, wlink), the maintained open
descendant of Watcom C/C++, which still produces 16-bit Windows NE binaries
and runs as a Linux cross compiler.

- Source: https://github.com/open-watcom/open-watcom-v2 , the "Current-build"
  release asset `ow-snapshot.tar.xz`.
- Extract it anywhere and point `WATCOM` at it. BUILD16.SH defaults to
  `/workspace/TOOLS/OW`:

      export WATCOM=/path/to/open-watcom
      sh BUILD16.SH

BUILD16.SH sets `PATH`, `INCLUDE` and the Watcom flags itself. It compiles
RETRO16.C, DECODE.C and AOUT.C (`wcc`, large model, `-0` for 8086, window
API), compiles the resources (`wrc`), and links an NE executable (`wlink`,
`system windows`). The result is DIST16/RETRO16.EXE.

Only the compiler is external; all RetroPlayer 16 source is in SRC16/. The
toolchain itself is not part of this archive (like the MinGW/QEMU tools used
for the 32-bit build).
