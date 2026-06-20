# Contributing to FLOW

Thanks for your interest in FLOW. This is a small, focused Windows-only project, so the setup is short.

> **Note on licensing:** FLOW is published "all rights reserved" (see [README](README.md#license)) — it is source-available, not open-source. By submitting a contribution you agree that it may be incorporated into the project under those same terms.

## Prerequisites

- **Windows 10 or 11 (64-bit)** — FLOW is Windows-only and uses raw Win32 APIs.
- **MinGW-w64 g++ (C++17)** on your `PATH`. The CI and releases use the MSYS2 `MINGW64` toolchain (`mingw-w64-x86_64-gcc`); a matching local install is recommended. MSVC is **not** supported.
- `windres` (ships with MinGW/MSYS2) to embed the icon + manifest.

Verify your toolchain:

```powershell
g++ --version      # should report a MinGW-w64 build
windres --version
```

## Build & run

```powershell
# Release (default) -> build\Release\FLOW.exe
.\scripts\build.ps1

# Debug (-g -O0) -> build\Debug\FLOW.exe
.\scripts\build.ps1 Debug
```

The raw invocation (what the script and CI run) is:

```
windres resource.rc -O coff -o build/resource.o
g++ -std=c++17 -O3 -mwindows -I include -o build/FLOW.exe \
    src/main.cpp src/FlowEngine.cpp build/resource.o \
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -lshell32 -static -static-libgcc -static-libstdc++
```

In VS Code, **Run Build Task** (`Ctrl+Shift+B`) runs the same thing.

### `-static` is mandatory

Without `-static`, the exe imports `libwinpthread-1.dll` (pulled in by `std::thread`/`std::mutex`) and fails to start on machines without MinGW. `-static` bundles the MinGW runtime into the exe; only system DLLs stay external. CI **fails the build** if any non-system DLL leaks into the imports or if the admin manifest isn't embedded — keep your changes static-clean.

### Running it

FLOW needs **Administrator privileges** for its global low-level input hooks. Launch an elevated terminal (or accept the UAC prompt). If `InstallHooks()` fails at startup, you almost certainly aren't running elevated.

### Debugging

`WinMain` calls `flow::protection::EnforceProtection()`, which silently exits the process if a debugger is detected. **Comment out that call** while you need to attach a debugger, and restore it before committing.

## Project layout

| Path | What it is |
|---|---|
| `src/main.cpp` | The Win32 GUI and all application state. |
| `src/FlowEngine.cpp` / `include/FlowEngine.h` | UI-agnostic engine (`flow` namespace). |
| `include/Protection.h` | Header-only anti-debugging. |
| `resource.rc` / `FLOW.manifest` | Icon + manifest, embedded via `windres`. |
| `scripts/` | `build.ps1`, `package.ps1`. |

See [`CLAUDE.md`](CLAUDE.md) for a deep dive into the architecture (engine subsystems, rendering model, macro file format, settings persistence).

## Coding conventions

- **C++17**, 4-space indentation, no tabs. An [`.editorconfig`](.editorconfig) is provided — please respect it.
- The build must be **warning-clean**: CI compiles with `-Wall -Wextra -Werror`. Don't introduce warnings.
- Engine code lives in `namespace flow` and must stay **UI-agnostic** (no window/HWND knowledge).
- **DPI:** every pixel literal in layout/geometry must be wrapped in `Sc(int)` / `Scf(float)`. An unwrapped coordinate is correct at 100% but misplaced at higher DPI.
- **GUI layout is manual.** `PaintUI` (painted text/dividers) and `CreateControls` (child widgets) read the **same** `constexpr` Y constants. If you move a control, move its label/divider too — they are not auto-laid-out.
- **MinGW `swprintf`:** use `%ls` for `wchar_t*` args (plain `%s` is treated as narrow and silently truncates).
- The binary **macro format** (`.rec`) serializes padded structs; changing `InputEvent` breaks existing saved files. Avoid changing it without a migration plan.

## Submitting changes

1. **Branch** off `master`.
2. **Build cleanly** (`.\scripts\build.ps1`) with no new warnings, and **run the elevated app** to confirm the affected feature actually works — there is no automated test suite, so manual verification is required.
3. Keep commits focused with clear messages.
4. Open a pull request and fill in the template. Describe what you changed and how you tested it.
5. If you changed behavior, update [`README.md`](README.md) and [`CLAUDE.md`](CLAUDE.md) to match.

## Reporting bugs & requesting features

Use the [issue templates](https://github.com/dominikkoenitzer/Flow/issues/new/choose). For anything that isn't a concrete bug or request, start a [Discussion](https://github.com/dominikkoenitzer/Flow/discussions). For security issues, follow [SECURITY.md](SECURITY.md) — please don't open a public issue.
