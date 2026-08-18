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
    src/*.cpp src/ui/*.cpp build/resource.o \
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -lshell32 -static -static-libgcc -static-libstdc++
```

In VS Code, **Run Build Task** (`Ctrl+Shift+B`) runs the same thing.

### `-static` is mandatory

Without `-static`, the exe imports `libwinpthread-1.dll` (pulled in by `std::thread`/`std::mutex`) and fails to start on machines without MinGW. `-static` bundles the MinGW runtime into the exe; only system DLLs stay external. CI **fails the build** if any non-system DLL leaks into the imports or if the admin manifest isn't embedded — keep your changes static-clean.

### Running it

FLOW needs **Administrator privileges** for its global low-level input hooks. Launch an elevated terminal (or accept the UAC prompt). If `InstallHooks()` fails at startup, you almost certainly aren't running elevated.

## Project layout

| Path | What it is |
|---|---|
| `src/main.cpp` | The window procedure, control creation and `WinMain` — the Win32 shell. |
| `src/AppState.cpp` / `include/AppState.h` | Control IDs, cached fonts, and the single `AppState` the whole GUI reads. |
| `src/Settings.cpp` / `include/Settings.h` | `%APPDATA%\FLOW\settings.cfg` load/save. |
| `src/Hotkeys.cpp` / `include/Hotkeys.h` | The four global hotkeys and key-name formatting. |
| `src/ui/Theme.h` | The design system: palette, layout grid, DPI scaling. |
| `src/ui/Draw.cpp` / `include/ui/Draw.h` | Anti-aliased GDI+ primitives and the vector glyphs. |
| `src/ui/Buttons.cpp` / `include/ui/Buttons.h` | The owner-draw buttons, toggles and key fields. |
| `src/ui/Dialogs.cpp` / `include/ui/Dialogs.h` | The hotkey-customization and About dialogs. |
| `src/FlowEngine.cpp` / `include/FlowEngine.h` | UI-agnostic engine (`flow` namespace). |
| `resource.rc` / `FLOW.manifest` | Icon + manifest, embedded via `windres`. |
| `scripts/` | `build.ps1`, `package.ps1`. |

## Tests

The engine has a [doctest](https://github.com/doctest/doctest) suite under `tests/`
covering the humanization jitter, the `.rec` file format (including the guards
against truncated and corrupt files), and the timing primitives.

```
.\scripts\test.ps1                       # build and run everything
.\scripts\test.ps1 -Filter "*macro*"     # one group
```

It links `FlowEngine.cpp` only, so it needs neither a window nor elevation. CI
runs the same compilation with `-Werror`.

`main.cpp` and `src/ui/` are not covered — they own `WinMain` and the painted
GUI, which still need manual verification.

## Coding conventions

- **C++17**, 4-space indentation, no tabs. An [`.editorconfig`](.editorconfig) is provided — please respect it.
- The build must be **warning-clean**: CI compiles with `-Wall -Wextra -Werror`. Don't introduce warnings.
- Engine code lives in `namespace flow` and must stay **UI-agnostic** (no window/HWND knowledge).
- **DPI:** every pixel literal in layout/geometry must be wrapped in `Sc(int)` / `Scf(float)`. An unwrapped coordinate is correct at 100% but misplaced at higher DPI.
- **GUI layout is manual.** `PaintUI` (painted text/dividers) and `CreateControls` (child widgets) read the **same** `constexpr` Y constants. If you move a control, move its label/divider too — they are not auto-laid-out.
- **MinGW `swprintf`:** use `%ls` for `wchar_t*` args (plain `%s` is treated as narrow and silently truncates).
- The binary **macro format** (`.rec`) serializes padded structs; changing `InputEvent` breaks existing saved files. Avoid changing it without a migration plan.

## Submitting changes

1. **Branch** off `main`.
2. **Build cleanly** (`.\scripts\build.ps1`) with no new warnings and **run the tests** (`.\scripts\test.ps1`). Engine changes should come with a test. GUI changes have no automated coverage, so **run the elevated app** and confirm the affected feature actually works.
3. Keep commits focused with clear messages.
4. Open a pull request and fill in the template. Describe what you changed and how you tested it.
5. If you changed behavior, update [`README.md`](README.md) to match.

## Reporting bugs & requesting features

Use the [issue templates](https://github.com/dominikkoenitzer/Flow/issues/new/choose). For anything that isn't a concrete bug or request, start a [Discussion](https://github.com/dominikkoenitzer/Flow/discussions). For security issues, follow [SECURITY.md](SECURITY.md) — please don't open a public issue.
