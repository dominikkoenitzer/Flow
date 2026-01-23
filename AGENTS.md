# AGENTS.md

## Project Overview
FLOW is a Windows automation tool similar to TinyTask, featuring macro recording/playback, auto-clicker, and humanization. Built with C++17 and Win32 API for native Windows GUI.

## Project Structure
```
flow/
├── src/
│   ├── main.cpp           ← Single GUI implementation
│   └── FlowEngine.cpp     ← Core automation engine
├── include/
│   ├── FlowEngine.h       ← Engine interface
│   └── Protection.h       ← Security headers
├── build/
│   └── FLOW.exe           ← Single executable output
└── AGENTS.md              ← This file
```

## Build Commands
- Build: `g++ -std=c++17 -O3 -mwindows -Wall -Wextra -Iinclude -o build/FLOW.exe src/main.cpp src/FlowEngine.cpp -luser32 -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++`
- Quick build: Press `Ctrl+Shift+B` in VS Code (runs "Build FLOW" task)
- Build output: Always `build/FLOW.exe` (overwrites previous)

## Code Style
- C++17 standard
- Win32 API for GUI (no external frameworks)
- Custom owner-drawn buttons with GDI drawing
- Single-file GUI implementation in `src/main.cpp`
- Naming: PascalCase for functions, camelCase for variables, g_ prefix for globals

## Critical Rules - ALWAYS FOLLOW

### Single Version Policy
- **ONLY ONE** version of each file exists at any time
- Main GUI file: `src/main.cpp` (never main_v2.cpp, main_new.cpp, etc.)
- Executable: `build/FLOW.exe` (never FLOW_v2.exe, FLOW_test.exe, etc.)
- **ALWAYS overwrite existing files when making changes**
- **NEVER create versioned files** (no _v2, _new, _old, _backup suffixes)

### File Modification Rules
- Edit `src/main.cpp` directly for ALL GUI changes
- Build always outputs to `build/FLOW.exe` (overwrites previous)
- If code breaks: revert or fix in the same file, don't create new versions
- Use git for version history, NOT filename suffixes

### Anti-Patterns (NEVER DO)
- ❌ Creating main_v2.cpp, main_new.cpp, main_icons_only.cpp
- ❌ Creating FLOW_v2.exe, FLOW_test.exe, FLOW_IconsOnly.exe
- ❌ Multiple build tasks for different "versions"
- ❌ Backup files cluttering workspace
- ❌ Asking "should I create a new version?" - answer is ALWAYS NO

## Development Workflow
1. Edit `src/main.cpp` directly
2. Build with `Ctrl+Shift+B` → outputs to `build/FLOW.exe` (overwrites)
3. Test the executable
4. If working: commit with git
5. If broken: revert with `git checkout src/main.cpp` or fix in same file

## Key Features
- Icon-only toolbar (Open, Save, Record, Play, Stop All, Settings)
- Status display showing current activity and playback speed
- Auto-clicker with configurable interval (F6 hotkey by default)
- Recording/playback with loop count (F7/F8 hotkeys by default)
- Customizable hotkeys - can reassign F1-F12 keys for each function
- Humanization with customizable timing variance
- Stop All button to halt all activities at once

## Hotkeys
- F6: Toggle auto-clicker (default, customizable)
- F8: Toggle recording (default, customizable)
- Ctrl+Shift+Alt+P: Toggle playback (default, customizable)
- Pause: Stop all activities (universal stop button)
- Customize recording/playback/clicker via Settings menu → "Customize Hotkeys..."

## Testing Instructions
- Build produces warnings (unused parameters, field initializers) - these are non-critical
- Test all hotkeys (F6, F7, F8) to ensure proper state management
- Verify Stop All button stops recording, playback, and clicking
- Test loop count and interval dialogs for proper input validation (1-999 for loops, 1-10000 for interval)

## Important Implementation Details
- Auto-clicker uses `Sleep(50)` after stopping for state synchronization
- Stop All button highlights when ANY activity is active (recording/playback/clicking)
- Loop count and click interval use modal dialogs with validation
- Status display updates in real-time showing current state and speed

## Remember
One codebase (`main.cpp`), one executable (`FLOW.exe`), one truth. Overwrite, don't create versions.
