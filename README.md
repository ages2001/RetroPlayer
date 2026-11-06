# RetroPlayer

A modern-codec media player for legacy Windows — from **Windows 3.0** all the
way to **Windows 11**. RetroPlayer wraps a patched **VLC 3.0.24 (libVLC)**
engine in a native, CRT-free Win32 frontend compiled for the plain **i386**
instruction set (no SSE/MMX), so it runs on machines and operating systems that
no mainstream player has supported for decades — while still decoding H.264,
HEVC, VP9 and AV1.

> There are two players: a 32-bit build (`RETROPLY.EXE`) for Windows NT 3.1
> through Windows 11, and a native 16-bit build (`RETRO16.EXE`) for Windows 3.0
> and 3.1.

---

## Supported operating systems

| Family | Versions | Player |
|--------|----------|--------|
| Windows 3.x | 3.0, 3.1 (16-bit) | `RETRO16.EXE` |
| Windows NT 3.x | NT 3.1, 3.5, 3.51 | `RETROPLY.EXE` |
| Windows NT 4.0 | NT 4.0 | `RETROPLY.EXE` |
| Windows 9x | 95, 98, 98 SE, Me | `RETROPLY.EXE` |
| Windows NT 5 | 2000, XP, XP x64, Server 2003 | `RETROPLY.EXE` |
| Windows NT 6+ | Vista, 7, 8, 10, 11 | `RETROPLY.EXE` |

The 32-bit player runs under WOW64 on the 64-bit editions (XP x64 and later).

---

## Features

- **Modern + legacy codecs** via the libVLC/FFmpeg core: H.264, HEVC, VP9, AV1,
  MPEG-1/2/4, VC-1/WMV, Theora, Cinepak, MJPEG and more; audio in AAC, MP3,
  FLAC, ALAC, Vorbis, Opus, AC-3/E-AC-3, WMA, WavPack, TTA and PCM; SRT and
  ASS/SSA (Unicode) subtitles.
- **VLC-style control bar** — draggable seek slider, elapsed/total time, a
  Play/Pause toggle with Stop and 10-second skips, a volume slider and mute.
- **Light, WMP/XP-inspired theme** that adapts to the display: glossy gradient
  controls on hi-color screens, and a palette-safe flat 3-D look on 16-color
  screens, so it looks right on everything from NT 3.1 to Windows 11.
- **Wizard installer** (welcome → destination → confirm → install) in both
  32-bit and 16-bit editions, with an editable install path.
- **System integration**:
  - *Windows 95 / NT 4 and later*: an **Add/Remove Programs** entry, a desktop
    shortcut, and a Program Manager / Start-menu shortcut — each with the app
    icon.
  - *Windows NT 3.x and Windows 3.x*: a **Program Manager** group holding the
    player shortcut and an uninstaller marked with a red-X icon.
- **Command-line / file-association playback** — pass a media path to open it
  directly (drag-and-drop and double-click friendly).

---

## Video output (per-OS "highest DirectX available")

RetroPlayer selects its video output by operating system, giving each one the
highest-DirectX path it can actually run, with a software fall-back chain
behind it. Direct3D 9 is only attempted where its runtime exists, and if no
Direct3D 9 device is available it falls through to DirectDraw and then to the
always-present software GDI output (`rpgdi`).

| Operating system | Output chain |
|------------------|--------------|
| Windows NT 3.x | GDI |
| Windows 95 | DirectDraw → GDI |
| Windows NT 4.0 | DirectDraw → GDI |
| Windows 98 / 98 SE / Me | Direct3D 9 → DirectDraw → GDI |
| Windows 2000 / XP / XP x64 / 2003 | Direct3D 9 → DirectDraw → GDI |
| Windows Vista / 7 / 8 / 10 / 11 | Direct3D 9 → DirectDraw → GDI |

The **Video** menu lets you override the choice at runtime (it sets the
`RP_VOUT` environment variable, which takes priority over the table above).

> **Note on acceleration:** the Direct3D 9 path needs a real 3-D capable GPU.
> Under emulation (and on adapters without 3-D) Direct3D 9 initialisation fails
> cleanly and the player drops to DirectDraw or GDI automatically, so playback
> always works — it simply isn't hardware-accelerated in that case.

---

## Repository layout

```
bin/        Ready-to-run installers
  RPSET32.EXE   32-bit wizard installer (NT 3.1 – Windows 11)
  RPSET16.EXE   16-bit installer (Windows 3.0 / 3.1)
src/        Full source tree and build scripts
  SRC/          32-bit frontend, installer and uninstaller (CRT-free Win32)
  SRC16/        16-bit frontend and installer (Open Watcom)
  SCRIPTS/      Build, packaging and QEMU test helpers
  TESTS/        Regression and media-decode tests
  PATCHES/      Patches applied to the VLC 3.0.24 engine
```

A separate ready-to-run package ships the unpacked player files instead of the
installers:

```
32/   32-bit player (RETROPLY.EXE, LIBCORE.DLL, PLUGINS\, RPUNIN.EXE, …)
16/   16-bit player (RETRO16.EXE, RPUN16.EXE)
```

---

## Installing

Run the installer that matches the target system:

- **`RPSET32.EXE`** on Windows NT 3.1 and later (including 9x). It installs to
  `C:\RETROPLY` on NT 3.x and to `%ProgramFiles%\Retro Player` on Windows 95 /
  NT 4 and later (`Program Files (x86)` on 64-bit editions).
- **`RPSET16.EXE`** on Windows 3.0 / 3.1.

Both are wizards: a welcome/info screen, an editable destination path, a
confirmation screen, and then installation. To remove the player, use
*Add/Remove Programs* (9x / NT 4+) or the uninstaller shortcut created in
Program Manager (NT 3.x / Windows 3.x).

Alternatively, from the ready-to-run package, copy the `32\` (or `16\`) folder
anywhere and run `RETROPLY.EXE` (or `RETRO16.EXE`) directly — the player is
self-contained.

---

## Building from source

The build uses a workspace-local MinGW i686 cross-toolchain (for the 32-bit
player and engine) and Open Watcom (for the 16-bit player); none of it is
installed system-wide.

```sh
sh SETUP.SH        # fetch and unpack the pinned toolchains (first time only)
sh BUILD.SH        # build the 32-bit player, DLLs and VLC engine
sh BUILD16.SH      # build the 16-bit player
sh INSTALL.SH      # build the installer stub and uninstaller
python3 SCRIPTS/INSPACK.PY    # append the player payload -> bin/RPSET32.EXE
python3 SCRIPTS/INSPK16.PY    # build the 16-bit installer   -> bin/RPSET16.EXE
```

The engine itself is a patched VLC 3.0.24 built for `i686-w64-mingw32` with
`-march=i386 -mno-sse -mno-sse2 -mno-mmx` and no C runtime dependency. The
OS-aware video-output change is documented in `src/PATCHES/VOUTSEL.DIF`.

---

## Testing

Each build is booted and exercised on period operating systems under QEMU
(IDE/AHCI disks, Cirrus VGA, emulated audio). Coverage includes Windows 3.0/3.1,
NT 3.1/3.51/4.0, Windows 95/98, Windows 2000, and Windows XP x64 — playing real
H.264/HEVC/VP9/AV1 clips on-screen, decoding a 24-case audio matrix and a
16-case container/codec matrix, and verifying the per-OS video-output selection
and its graceful fall-back. `src/SCRIPTS` contains the QEMU launch and
automation helpers; `src/STATUS.MD` records the exact validation scope per
operating system.

---

## License

RetroPlayer's own source code is released under the **GNU General Public
License, version 2 or (at your option) any later version**
(`GPL-2.0-or-later`). The full license text is in the [`LICENSE`](LICENSE)
file. The legacy `rpgdi` software video-output module is
`LGPL-2.1-or-later`, as stated in its source.

Copyright © 2026 Atakan ("ages2001").

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Third-party components

RetroPlayer bundles a patched build of **VLC / libVLC 3.0.24** and its
FFmpeg-based decoders, plus other open-source libraries, each under its own
license (VLC core and `rpgdi`: LGPL-2.1+; the VLC application layer and some
plugins: GPL-2.0+; FFmpeg: LGPL-2.1+; dav1d: BSD; and others). The individual
license texts are collected under the `LICENSES/` folder of the player
distribution, and `LICENSE.TXT` summarises how they apply. Upstream sources
keep their original copyright notices.
