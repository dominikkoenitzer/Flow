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
    -luser32 -lgdi32 -lcomctl32 -lgdiplus -static-libgcc -static-libstdc++
```

Notes:
- `windres resource.rc -O coff -o build/resource.o` embeds the app icon; the build still succeeds without it (icon just won't appear).
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

The window is a **fixed-size, card-based layout** (light theme): a header (wordmark + live status pill), three cards (RECORD / AUTO-CLICKER / PLAYBACK), and a footer (Open / Save / Settings / Stop All). Client size and all card/control rects are derived from `constexpr` layout constants (`PAD`, `GAP`, `COL_W`, `CARDS_TOP`, `REC_TOP`, etc.) near the top of the file — change geometry there, and keep `PaintUI` and `CreateControls` in sync since they use the same constants.

- **Rendering model**: the parent paints itself in a double-buffered `WM_PAINT` (`PaintUI`) — background, header divider, status pill, the three cards, and all static label text. `WM_ERASEBKGND` returns 1 (no flicker). The window has `WS_CLIPCHILDREN`, so child controls paint themselves on top and are never overwritten by the parent blit. Anti-aliased shapes (rounded cards w/ soft shadow, button fills, icons, pill) are drawn with **GDI+** via helpers `FillRound` / `StrokeRound` / `DrawCard` / `Icon*`; text is GDI (`TextLine`) using the cached `g_fonts`. GDI+ is initialized in `WinMain` (`GdiplusStartup`/`Shutdown`).
- **Buttons** are owner-drawn (`BS_OWNERDRAW` + `WM_DRAWITEM` → `DrawFlowButton`); appearance is resolved from the control ID, not stored per-button. Record/Play/Clicker are toggles (ghost when idle, filled accent when active); Stop All is always filled-danger; Open/Save/Settings are ghost.
- **Inline settings are real child controls** on the cards: `EDIT_SPEED` / `EDIT_LOOPS` / `EDIT_INTERVAL` (committed on `EN_KILLFOCUS` via `Apply*Edit`, clamped + reformatted) and `CHK_CONTINUOUS` / `CHK_HUMANIZE` checkboxes. `WM_CTLCOLOREDIT`/`WM_CTLCOLORSTATIC` return a white brush so inputs/checkboxes blend into the white cards. State changes call `UpdateStatusDisplay()`, which does `RedrawWindow(... RDW_INVALIDATE | RDW_ALLCHILDREN)` so both the painted surface and the owner-draw buttons refresh.
- The **Settings button** opens a small popup menu (`ShowSettingsMenu`) with only the occasional actions: Customize Hotkeys, Always on Top, Clear Recording, About. The **Customize Hotkeys** and **About** dialogs share the same approach: built programmatically from an inline `DLGTEMPLATE` + manual child controls + a local modal loop (`EnableWindow(parent,FALSE)` + `GetMessage`/`IsDialogMessage`), themed to match the main window (class background `BG_PRIMARY`, cached `g_fonts`, muted/dark text resolved in `WM_CTLCOLORSTATIC` by control ID, owner-draw rounded buttons via `DrawDlgButton`). There is no `.rc` dialog resource.
- Global hotkeys are registered with `RegisterHotKey` and handled in `WM_HOTKEY`. Defaults: F8 record, Ctrl+Shift+Alt+P playback, F6 auto-clicker, Pause stop-all (all customizable). A `WM_TIMER` (every 100ms) polls `IsPlaybackActive()` to reset UI state when a finite playback finishes on its own.
- **Settings persistence**: `LoadSettings()`/`SaveSettings()` (top of `main.cpp`) read/write `%APPDATA%\FLOW\settings.cfg` (simple `key=value` lines). Loaded in `WinMain` before hotkey registration / control creation (and applied to the engine), saved on `WM_DESTROY`. Persisted keys: playback speed, click interval, loop count, continuous, always-on-top, humanization on/off + std-dev, and the four hotkeys.

### Protection

`WinMain` calls `flow::protection::EnforceProtection()` at startup, which silently `ExitProcess(1)`s if a debugger is detected (`IsDebuggerPresent`, `CheckRemoteDebuggerPresent`, known debugger window classes, or a timing check). **Running FLOW under a debugger will cause it to exit immediately** — comment out that call when you need to debug.

## Gotchas

- Playback speed is applied in `PlaybackThreadFunction` by dividing each inter-event delay by `playbackSpeed` (an engine `atomic<double>`, set via `SetPlaybackSpeed`). It's read live, so changing speed mid-playback takes effect immediately. The auto-clicker is unaffected (it uses an explicit interval).
- The Customize Hotkeys dialog's edit fields only accept F1–F12, A–Z, and 0–9. The default Stop key is `Pause`, which therefore can't be *re-typed* once changed — only replaced with an accepted key.
- Do **not** call `PostQuitMessage` from a dialog proc (it once quit the whole app from the hotkey dialog's `WM_DESTROY`).
- **DPI scaling**: layout constants and font sizes are design units at 96 DPI. `g_scale` is computed once in `WinMain` from the primary monitor's DPI (`GetDeviceCaps(LOGPIXELSX)/96`), and every pixel value is wrapped in `Sc(int)` / `Scf(float)`. So when editing geometry, **always wrap literals in `Sc()`** — an unscaled coordinate will be correct at 100% but misplaced at higher DPI. The window does not re-scale live on monitor moves (no `WM_DPICHANGED` handler); scale is fixed at the primary monitor's DPI at launch.
- `PaintUI` draws the card label text; the interactive widgets are separate child controls positioned with the same constants. If you move a control in `CreateControls`, move its label in `PaintUI` too (they are not auto-laid-out).
