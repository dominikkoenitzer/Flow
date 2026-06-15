# FLOW

**Flexible Low-latency Operations Workflow** — a fast, lightweight input-automation tool for Windows. Record and replay mouse/keyboard macros and run a high-speed auto-clicker, all from a custom-drawn native UI with no external runtime to install.

> ⚠️ FLOW must be run **as Administrator**. It installs global low-level input hooks, which Windows only permits for elevated processes. The downloaded `.exe` requests elevation automatically (UAC prompt).

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
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -static -static-libgcc -static-libstdc++
```

The `-static` flag is required so the resulting exe carries the MinGW runtime
(`libwinpthread`, `libstdc++`, `libgcc`) internally and runs on machines without MinGW.

## Cutting a release

Releases are **named, not numbered** — each one gets a codename. This first release is **Firefly**.

Releases are fully automated by [`.github/workflows/release.yml`](.github/workflows/release.yml). Push a tag with the release name and the workflow builds, verifies the binary is self-contained, and publishes a **draft** GitHub Release titled *FLOW — "Firefly"* with the exe, zip, and checksums attached:

```powershell
git tag firefly
git push origin firefly
```

Then open the draft on the Releases page, review it, and click **Publish**. The next release simply gets a new name (e.g. `git tag mayfly`).

To build the release artifacts locally without GitHub, run [`scripts/package.ps1`](scripts/package.ps1).

## Disclaimer

FLOW is intended for legitimate automation of repetitive tasks. You are responsible for complying with the terms of service of any software you use it with.
