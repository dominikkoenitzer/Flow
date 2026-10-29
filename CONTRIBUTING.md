# Contributing to FLOW

Thanks for your interest in FLOW. This is a small, focused Windows-only project, so the setup is short.

> **Note on licensing:** FLOW is published "all rights reserved" (see [README](README.md#license)), so it is source-available, not open-source. By submitting a contribution you agree that it may be incorporated into the project under those same terms.

## Prerequisites

- **Windows 10 or 11 (64-bit).** FLOW is Windows-only and uses raw Win32 APIs.
- **MinGW-w64 g++ (C++17)** on your `PATH`. The CI and releases use the MSYS2 `UCRT64` toolchain (`mingw-w64-ucrt-x86_64-gcc`); a matching local install is recommended. MSVC is **not** supported.
- `windres` (ships with MinGW/MSYS2) to embed the icon + manifest.

For a setup from scratch on a fresh Windows PC, follow [Build from source](README.md#build-from-source) in the README.

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

The raw invocation CI runs, in PowerShell from the repo folder, is below. The script compiles the same sources with the same libraries but without `-Wall -Wextra -Werror`:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
windres resource.rc -O coff -o build/resource.o
$src = (Get-ChildItem src -Recurse -Filter *.cpp).FullName
g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Werror -mwindows -I include -o build/FLOW.exe $src build/resource.o -luser32 -lgdi32 -lcomctl32 -lgdiplus -lshell32 -lole32 -luuid -static -static-libgcc -static-libstdc++
```

In VS Code, **Run Build Task** (`Ctrl+Shift+B`) builds with `-Wall -Wextra`, but without `-Werror` and `-DNDEBUG`.

### `-static` is mandatory

Without `-static`, the exe imports `libwinpthread-1.dll` (pulled in by `std::thread`/`std::mutex`) and fails to start on machines without MinGW. `-static` bundles the MinGW runtime into the exe; only system DLLs stay external. CI **fails the build** if any non-system DLL leaks into the imports or if the admin manifest isn't embedded, so keep your changes static-clean.

### Running it

FLOW needs **Administrator privileges** for its global low-level input hooks. Launch an elevated terminal (or accept the UAC prompt). If `InstallHooks()` fails at startup, you almost certainly aren't running elevated.

## Project layout

| Path | What it is |
|---|---|
| `src/main.cpp` | The window procedure, control creation and `WinMain`: the Win32 shell. |
| `src/AppState.cpp` / `include/AppState.h` | Control IDs, cached fonts, and the single `AppState` the whole GUI reads. |
| `src/Settings.cpp` / `include/Settings.h` | `%APPDATA%\FLOW\settings.cfg` load/save. |
| `src/Hotkeys.cpp` / `include/Hotkeys.h` | The four global hotkeys and key-name formatting. |
| `include/ui/Theme.h` | The design system: palette, layout grid, DPI scaling. |
| `src/ui/Dpi.cpp` / `include/ui/Dpi.h` | Per-monitor DPI lookups and frame sizes, so each window follows its monitor's scale. |
| `include/ui/Placement.h` | Keeps a restored window inside its monitor's work area. |
| `include/ui/NumberFields.h` | The number fields, and taking the focused one's value before a hotkey acts. |
| `src/ui/Draw.cpp` / `include/ui/Draw.h` | Anti-aliased GDI+ primitives and the vector glyphs. |
| `src/ui/Buttons.cpp` / `include/ui/Buttons.h` | The owner-draw buttons, toggles and key fields. |
| `src/ui/Dialogs.cpp` / `include/ui/Dialogs.h` | The hotkey-customization, Trim & Timing and About dialogs. |
| `src/FlowEngine.cpp` / `include/FlowEngine.h` | UI-agnostic engine (`flow` namespace). |
| `resource.rc` / `FLOW.manifest` | Icon + manifest, embedded via `windres`. |
| `scripts/` | `build.ps1`, `test.ps1`, `package.ps1`, `make-icon.ps1` (redraws `Flow.ico` and `Flow.png`). |

## Tests

The engine has a [doctest](https://github.com/doctest/doctest) suite under `tests/`
covering the humanization jitter, the `.rec` file format (including the guards
against truncated and corrupt files), the timing primitives, the auto-clicker's
options and thread lifecycle, and the clamp that keeps a restored window inside
its work area.

```
.\scripts\test.ps1                       # build and run everything
.\scripts\test.ps1 -Filter "*macro*"     # one group
```

It links `FlowEngine.cpp` only, so it needs neither a window nor elevation. CI
runs the same compilation with `-Werror`.

`main.cpp` and `src/ui/` are not covered. They own `WinMain` and the painted
GUI, which still need manual verification.

## Coding conventions

- **C++17**, 4-space indentation, no tabs. There is an [`.editorconfig`](.editorconfig); please respect it.
- The build must be **warning-clean**: CI compiles with `-Wall -Wextra -Werror`. Don't introduce warnings.
- Engine code lives in `namespace flow` and must stay **UI-agnostic** (no window/HWND knowledge).
- **DPI:** every pixel literal in layout/geometry must be wrapped in `Sc(int)` / `Scf(float)`. An unwrapped coordinate is correct at 100% but misplaced at higher DPI. Set a control's position and size only in `LayoutControls` (main window) or the dialog's `Layout...Dialog` function: those run again when the window moves to a monitor with another scale, and anything placed elsewhere stays at the old one.
- **GUI layout is manual.** `PaintUI` (painted text/dividers) and `CreateControls` (child widgets) read the **same** `constexpr` Y constants. If you move a control, move its label and divider too. Nothing here is auto-laid-out.
- **MinGW `swprintf`:** use `%ls` for `wchar_t*` args (plain `%s` is treated as narrow and silently truncates).
- The binary **macro format** (`.rec`) is versioned and written field by field; the layout is documented in `include/FlowEngine.h`. A new field means a new `MACRO_FORMAT_VERSION`, and `LoadMacro` must keep reading every older version.

## Submitting changes

1. **Branch** off `main`.
2. **Build cleanly**: the raw invocation above must compile without warnings, because CI treats every warning as an error. Then **run the tests** (`.\scripts\test.ps1`). Engine changes should come with a test. GUI changes have no automated coverage, so **run the elevated app** and confirm the affected feature actually works.
3. Keep commits focused with clear messages.
4. Open a pull request and fill in the template. Describe what you changed and how you tested it.
5. If you changed behavior, update [`README.md`](README.md) to match.

## Reporting bugs & requesting features

Use the [issue templates](https://github.com/dominikkoenitzer/Flow/issues/new/choose). For security issues, follow [SECURITY.md](SECURITY.md), and please don't open a public issue.
