# FLOW

**Flexible Low-latency Operations Workflow** — a fast, lightweight input-automation tool for Windows. Record and replay mouse/keyboard macros and run a high-speed auto-clicker, all from a custom-drawn native UI with no external runtime to install.

[![CI](https://github.com/dominikkoenitzer/Flow/actions/workflows/ci.yml/badge.svg)](https://github.com/dominikkoenitzer/Flow/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/dominikkoenitzer/Flow?display_name=tag&label=latest)](https://github.com/dominikkoenitzer/Flow/releases/latest)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?logo=windows&logoColor=white)](#requirements)
[![Language](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](#build-from-source)
[![License](https://img.shields.io/badge/license-all%20rights%20reserved-lightgrey)](#license)

> ⚠️ FLOW must be run **as Administrator**. It installs global low-level input hooks, which Windows only permits for elevated processes. The downloaded `.exe` requests elevation automatically (UAC prompt).

## Features

- **Macro recording & playback** — capture mouse moves, clicks, and keystrokes with millisecond timing, then replay them with adjustable speed and loop count (including infinite loops).
- **High-speed auto-clicker** — a dedicated, high-priority clicker at a configurable interval, independent of recorded macros.
- **Low-latency timing** — a busy-wait `QueryPerformanceCounter` timer delivers sub-10 ms precision that plain `Sleep` can't.
- **Humanization** — optional Gaussian jitter on delays to avoid fixed-interval patterns.
- **Multi-monitor aware** — mouse moves replay as absolute, virtual-desktop-normalized coordinates, so playback lands correctly across displays.
- **Native, dependency-free UI** — a custom-drawn Win32 + GDI+ window with a light theme, owner-drawn buttons, switches, and inline numeric fields. No framework, no runtime to install.
- **Quality-of-life** — global hotkeys (fully customizable), drag-and-drop `.rec` files, reopen-last-macro on launch, and a remembered window position.

## Download

Grab the latest build from the [**Releases**](https://github.com/dominikkoenitzer/Flow/releases) page:

- **`FLOW.exe`** — the standalone executable (recommended). Fully self-contained, no dependencies.
- **`FLOW-<name>-win64.zip`** — the same exe plus this README.
- **`SHA256SUMS-<name>.txt`** — checksums to verify your download.

### "Windows protected your PC" (SmartScreen)

FLOW is not code-signed, so SmartScreen may warn on first launch. This is expected for a new indie tool. To run it:

1. Click **More info**.
2. Click **Run anyway**.

If you want to be certain the download is intact, verify the checksum:

```powershell
Get-FileHash .\FLOW.exe -Algorithm SHA256
# compare against the value in SHA256SUMS-<name>.txt
```

Some antivirus engines may also flag automation tools that use global input hooks. This is a false positive inherent to this category of software.

## Requirements

- Windows 10 or Windows 11 (64-bit)
- Administrator rights (for input hooks)

No Visual C++ redistributable, no MinGW runtime, nothing else — the binary is statically linked.

## Default hotkeys

| Action | Key |
|---|---|
| Start/stop recording | `F8` |
| Start/stop playback | `F9` |
| Toggle auto-clicker | `F6` |
| Stop everything | `Pause` |

All hotkeys are customizable in **Settings → Customize Hotkeys**.

## Tips

- **Drag & drop** a `.rec` file onto the window to load it instantly.
- Turn on **Settings → Reopen Last Macro on Launch** to have your last macro ready every time you start FLOW.
- FLOW **remembers its window position** and reopens where you left it.

## Build from source

The toolchain is **MinGW-w64 g++** (C++17). With MinGW on your `PATH`:

```powershell
# Release (default) -> build\Release\FLOW.exe
.\scripts\build.ps1

# Debug -> build\Debug\FLOW.exe
.\scripts\build.ps1 Debug
```

Or the raw invocation:

```
windres resource.rc -O coff -o build/resource.o
g++ -std=c++17 -O3 -mwindows -I include -o build/FLOW.exe \
    src/main.cpp src/FlowEngine.cpp build/resource.o \
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -lshell32 -static -static-libgcc -static-libstdc++
```

The `-static` flag is required so the resulting exe carries the MinGW runtime
(`libwinpthread`, `libstdc++`, `libgcc`) internally and runs on machines without MinGW.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full development setup, coding conventions, and how to test changes.

## Project structure

| Path | What it is |
|---|---|
| `src/main.cpp` | The entire Win32 GUI and all application state; owns the engine. |
| `src/FlowEngine.cpp` / `include/FlowEngine.h` | UI-agnostic engine (`flow` namespace): recording, playback, auto-clicker, timing, humanization. |
| `include/Protection.h` | Header-only anti-debugging helpers. |
| `resource.rc` / `FLOW.manifest` | App icon + manifest (admin elevation, visual styles, DPI awareness), embedded via `windres`. |
| `scripts/build.ps1` | Build script (Release/Debug) wrapping the g++ invocation. |
| `scripts/package.ps1` | Produces the release artifacts (zip + exe + SHA-256) locally. |
| `.github/workflows/ci.yml` | Build + self-contained/manifest verification on every push & PR. |
| `.github/workflows/release.yml` | Builds and publishes the rolling `flow` GitHub Release when the tag is pushed. |
| `CLAUDE.md` | In-depth architecture notes for contributors (and AI assistants). |

## Cutting a release

There's a single, **unversioned** release — **`flow`** — that always holds the latest build. No version numbers, no per-release codenames.

Releases are fully automated by [`.github/workflows/release.yml`](.github/workflows/release.yml). To (re)publish, force-move the `flow` tag onto the latest commit and push it; the workflow builds, verifies the binary is self-contained, and **updates** the GitHub Release titled *FLOW* with the exe, zip, and checksums attached:

```powershell
git tag -f flow
git push -f origin flow
```

The release updates automatically once CI passes (the build is verified self-contained before anything is published).

To build the release artifacts locally without GitHub, run [`scripts/package.ps1`](scripts/package.ps1).

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) for the build/run setup and coding conventions, and open an issue or discussion before large changes. Security issues should follow [SECURITY.md](SECURITY.md).

## License

**All rights reserved.** No license is granted for reuse, modification, or redistribution of this code. You may view the source here, but it is not open-source software. If you'd like to use part of FLOW, please open an issue to ask.

## Disclaimer

FLOW is intended for legitimate automation of repetitive tasks. You are responsible for complying with the terms of service of any software you use it with.
