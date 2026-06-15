# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FLOW (Flexible Low-latency Operations Workflow) is a Windows-only C++17 input automation tool. It records and replays mouse/keyboard macros, provides a high-speed auto-clicker, and renders a custom-drawn Win32 GUI. There is no third-party UI framework — the window is raw Win32 with GDI+ used for anti-aliased rendering, and the engine is raw Win32 input APIs.

## Build & Run

The toolchain is **MinGW-w64 g++** (not MSVC, despite the `.gitignore` referencing Visual Studio/CMake — there is no CMakeLists). Two equivalent build paths:

```powershell
# PowerShell build script (Release is default; pass Debug for -g -O0)
.\scripts\build.ps1            # -> build\Release\FLOW.exe
.\scripts\build.ps1 Debug      # -> build\Debug\FLOW.exe
```

```
# VS Code: Terminal > Run Build Task (Ctrl+Shift+B) runs "Build FLOW"
#   - "Compile Resource" (windres resource.rc -> build/resource.o) runs first
#   - "Build and Run FLOW" builds then launches the exe
```

The manual g++ invocation (what the script/task run):
```
g++ -std=c++17 -O3 -mwindows -I include -o build/FLOW.exe \
    src/main.cpp src/FlowEngine.cpp build/resource.o \
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -lshell32 -static -static-libgcc -static-libstdc++
```

**`-static` is mandatory for distribution.** Without it the exe imports `libwinpthread-1.dll` (pulled in by `std::thread`/`std::mutex`) and fails to start on any machine without MinGW installed. `-static` bundles libwinpthread/libstdc++/libgcc into the exe; only system DLLs (user32, gdi32, …) stay external.

Notes:
- `windres resource.rc -O coff -o build/resource.o` embeds the app icon **and `FLOW.manifest`** (admin elevation + visual styles + DPI awareness) into `resource.o`. The build still succeeds without it, but the resulting exe won't auto-elevate or get visual styles — so end users who double-click it hit the "Failed to install input hooks" error. Always build with the resource object for release.
- **Releases are automated** by `.github/workflows/release.yml`: releases are **named, not numbered** (e.g. `Firefly`), so pushing any tag builds a static binary, verifies it has no non-system DLL imports and an embedded manifest, then **publishes** a GitHub Release titled `FLOW — "<Name>"` with the exe, zip, and SHA-256 checksums attached. `scripts/package.ps1` produces the same artifacts locally.
- There are **no tests** and **no linter** configured.
- The app **requires Administrator privileges** (declared in `FLOW.manifest` and required for global low-level hooks). `WinMain` aborts with an error box if `InstallHooks()` fails — that failure almost always means it wasn't run elevated.

## Architecture

Three files carry all the logic:

- **`include/FlowEngine.h` / `src/FlowEngine.cpp`** — the engine, in namespace `flow`. UI-agnostic; knows nothing about windows.
- **`src/main.cpp`** — the entire Win32 GUI plus all application state. Owns the `FlowEngine`.
- **`include/Protection.h`** — header-only anti-debugging (inline functions, no .cpp).

### Engine (`FlowEngine`) — singleton driving three independent subsystems

`FlowEngine` is effectively a singleton: the constructor stores `this` in the static `FlowEngine::instance`, and the static Win32 hook callbacks (`MouseHookProc`/`KeyboardHookProc`) route events back through that pointer. **Only one instance may exist per process.** The three subsystems each run on their own worker thread and are coordinated with atomics:

1. **Recording** — `InstallHooks()` installs global `WH_MOUSE_LL` / `WH_KEYBOARD_LL` hooks. While `isRecording` is set, callbacks append `InputEvent`s to `recordedEvents` (guarded by `recordMutex`) with `GetTickCount()`-relative timestamps. `OnKeyboardEvent` deliberately drops VK_F6 / VK_F8 / 'P' so the control hotkeys aren't captured into the macro.
2. **Auto-clicker** — `ClickerThreadFunction` runs at `THREAD_PRIORITY_HIGHEST`, issuing `SendInput` left-click pairs at `clickInterval` ms, optionally jittered by the humanizer.
3. **Playback** — `PlaybackThreadFunction` copies `recordedEvents` under the mutex (to avoid racing the recorder), then replays via `SendInput`, sleeping the inter-event delta between events. Loops `loopCount` times (`-1` = infinite). Mouse moves are replayed as **absolute, virtual-desktop-normalized** coordinates (0–65535 with `MOUSEEVENTF_VIRTUALDESK`) for correct multi-monitor placement.

Supporting pieces:
- **`HighResTimer::PreciseSleep`** — busy-waits on `QueryPerformanceCounter` for sub-10ms precision; longer delays use `Sleep(n-2)` then busy-wait the remainder. This is the source of the "low-latency" timing.
- **`HumanizationEngine`** — adds Gaussian (`mt19937` + `normal_distribution`) variance to delays to defeat fixed-interval pattern detection. Mutex-guarded; enabled by default in the engine.

### Macro file format (`SaveMacro`/`LoadMacro`)

Binary: 4-byte magic `"FLOW"`, then `size_t` event count, then raw `InputEvent` structs written with `memcpy` semantics (`file.write(&event, sizeof(InputEvent))`). **This format is compiler/architecture-specific** (it serializes padded structs including `POINT`), so it is not portable and changing `InputEvent` breaks existing saved files. The Open/Save dialogs use a `*.rec` extension filter.

### GUI (`main.cpp`)

The window is a **fixed-size, flat single-column layout** (light theme): a header (wordmark + tagline + live status dot/label), then three stacked sections separated by hairline dividers — RECORD, PLAYBACK, AUTO-CLICKER — and a footer (Open / Save / Settings / Stop All). There are **no nested cards**; the whole window is one `BG_PRIMARY` surface. Every element's position is an explicit `constexpr` Y constant (`REC_DIV_Y`, `REC_BTN_Y`, `PLAY_BTN_Y`, `SPEED_ROW_Y`, `TOG1_Y`, `FOOT_Y`, …) near the top of the file, plus `CONTENT_X`/`CONTENT_W`. **`PaintUI` (painted text/dividers) and `CreateControls` (child widgets) read the same constants — keep them in sync.**

- **Rendering model**: the parent paints itself in a double-buffered `WM_PAINT` (`PaintUI`) — background, dividers, status dot+label, header, section labels, the record info/empty-state line, inline field labels, and the derived "Est. runtime"/"clicks-sec" helpers. `WM_ERASEBKGND` returns 1 (no flicker). `WS_CLIPCHILDREN` keeps child controls from being overwritten by the parent blit. Anti-aliased shapes are **GDI+** (`FillRound` / `StrokeRound` / `Icon*`); text is GDI (`TextLine`) with cached `g_fonts`. GDI+ is started/stopped in `WinMain`.
- **Button hierarchy** (owner-drawn, `WM_DRAWITEM` → `DrawFlowButton`, resolved by control ID into a `Role`): there is **one saturated hero at a time**. `Role::Hero` = filled accent (Record idle, Play once a macro exists, any active Stop). `Role::Disabled` = grey — Play is disabled (`EnableWindow` toggled in `UpdateStatusDisplay`/`CreateControls`) until `GetEventCount()>0`. `Role::Secondary` = white + accent outline — the auto-clicker's Start (a different mode), which fills accent only while running. `Role::Ghost` = footer + idle Stop All (no icon/colour until something runs). Hero/secondary buttons left-align icon+label and right-align a live **hotkey hint** (`GetKeyName`); `Darken()` makes pressed shades.
- **Toggles**: `CHK_CONTINUOUS` / `CHK_HUMANIZE` are owner-drawn switch controls (`DrawToggle`), not checkboxes — state lives in `g_app` and is flipped in `WM_COMMAND`/`BN_CLICKED`, then the button is invalidated.
- **Inline numeric fields**: `EDIT_SPEED` / `EDIT_LOOPS` / `EDIT_INTERVAL` are **borderless, right-aligned** monospace (`g_fonts.mono`) edits over a rounded pill drawn by `PaintUI` (ghost-stepper affordance: hairline at rest, `TRACK_HOVER` fill on hover, accent ring on focus). Hover is tracked by subclassing each edit with `InputEditProc` (`g_hoverEdit` + `TrackMouseEvent`); focus/blur and hover all `InvalidateRect` the parent so the pill repaints. Values commit on `EN_KILLFOCUS` via `Apply*Edit` (clamped + reformatted). State changes call `UpdateStatusDisplay()` → `RedrawWindow(... RDW_INVALIDATE | RDW_ALLCHILDREN)`. **MinGW `swprintf` note:** use `%ls` for `wchar_t*` args (plain `%s` is treated as narrow and silently truncates). Section labels use the neutral `LABEL_GREY` (kept distinct from the accent-blue clicker outline).
- The **Settings button** opens a small popup menu (`ShowSettingsMenu`) with only the occasional actions: Customize Hotkeys, Always on Top, Reopen Last Macro on Launch, Clear Recording, About. The **Customize Hotkeys** and **About** dialogs share the same approach: built programmatically from an inline `DLGTEMPLATE` + manual child controls + a local modal loop (`EnableWindow(parent,FALSE)` + `GetMessage`/`IsDialogMessage`), themed to match the main window (class background `BG_PRIMARY`, cached `g_fonts`, muted/dark text resolved in `WM_CTLCOLORSTATIC` by control ID, owner-draw rounded buttons via `DrawDlgButton`). The hotkey dialog's four key fields are **owner-draw buttons** (not edits) rendered by `DrawKeyField` as the same soft pill as the main inputs (mono text, accent ring via `ODS_FOCUS`); key capture is the `HotkeyEditProc` subclass writing `g_tempHotkey*` and invalidating. There is no `.rc` dialog resource.
- Global hotkeys are registered with `RegisterHotKey` and handled in `WM_HOTKEY`. Defaults: F8 record, F9 playback, F6 auto-clicker, Pause stop-all (all customizable; `hotkeyModifiers` is 0 by default). A `WM_TIMER` (every 100ms) polls `IsPlaybackActive()` to reset UI state when a finite playback finishes on its own.
- **Settings persistence**: `LoadSettings()`/`SaveSettings()` (top of `main.cpp`) read/write `%APPDATA%\FLOW\settings.cfg` (simple `key=value` lines). `LoadSettings()` runs early in `WinMain` (right after class registration, **before** the window is created) so the saved window position can be restored; settings are saved on `WM_DESTROY`. Persisted keys: playback speed, click interval, loop count, continuous, always-on-top, humanization on/off + std-dev, the four hotkeys, plus `reopenLastMacro`, `lastMacro` (UTF-8 path via `WideToUtf8`/`Utf8ToWide`), and `winX`/`winY`.
- **Quality-of-life**: the window accepts **drag-and-drop** of a `.rec` file (`DragAcceptFiles` + `WM_DROPFILES` → `LoadMacro`; needs `-lshell32`). With *Reopen Last Macro on Launch* enabled, the last opened/saved/dropped macro is reloaded at startup. The window position is remembered and restored on next launch (clamped to the virtual desktop so it can't open off-screen).

### Protection

`WinMain` calls `flow::protection::EnforceProtection()` at startup, which silently `ExitProcess(1)`s if a debugger is detected (`IsDebuggerPresent`, `CheckRemoteDebuggerPresent`, known debugger window classes, or a timing check). **Running FLOW under a debugger will cause it to exit immediately** — comment out that call when you need to debug.

## Gotchas

- Playback speed is applied in `PlaybackThreadFunction` by dividing each inter-event delay by `playbackSpeed` (an engine `atomic<double>`, set via `SetPlaybackSpeed`). It's read live, so changing speed mid-playback takes effect immediately. The auto-clicker is unaffected (it uses an explicit interval).
- The Customize Hotkeys dialog's edit fields only accept F1–F12, A–Z, and 0–9. The default Stop key is `Pause`, which therefore can't be *re-typed* once changed — only replaced with an accepted key.
- Do **not** call `PostQuitMessage` from a dialog proc (it once quit the whole app from the hotkey dialog's `WM_DESTROY`).
- **DPI scaling**: layout constants and font sizes are design units at 96 DPI. `g_scale` is computed once in `WinMain` from the primary monitor's DPI (`GetDeviceCaps(LOGPIXELSX)/96`), and every pixel value is wrapped in `Sc(int)` / `Scf(float)`. So when editing geometry, **always wrap literals in `Sc()`** — an unscaled coordinate will be correct at 100% but misplaced at higher DPI. The window does not re-scale live on monitor moves (no `WM_DPICHANGED` handler); scale is fixed at the primary monitor's DPI at launch.
- `PaintUI` draws the card label text; the interactive widgets are separate child controls positioned with the same constants. If you move a control in `CreateControls`, move its label in `PaintUI` too (they are not auto-laid-out).
