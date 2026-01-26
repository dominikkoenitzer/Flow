# FLOW - Professional Automation Tool

A modern, lightweight Windows automation tool for recording and playing back mouse and keyboard actions.

## Features

- 🎯 **Auto-Clicker** - Automated clicking with F6 hotkey
- 📹 **Action Recording** - Record mouse clicks and keyboard inputs
- ▶️ **Playback** - Replay recorded actions with precise timing
- ⚡ **Configurable** - Adjustable intervals, loop counts, and hotkeys
- 🎨 **Modern UI** - Clean, professional interface
- 🔒 **Lightweight** - Single executable, no dependencies

## System Requirements

- Windows 10/11
- Must run as Administrator for input hooks

## Building from Source

### Prerequisites
- MinGW-w64 (g++ compiler)
- windres (for resource compilation)

### Build Steps

```powershell
# Run the build script
.\scripts\build.ps1

# Or build manually
windres resource.rc -O coff -o build\resource.o
g++ -std=c++17 -O3 -mwindows -Wall -Wextra -Iinclude ^
    -o build\FLOW.exe src\main.cpp src\FlowEngine.cpp build\resource.o ^
    -luser32 -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++
```

### Using VS Code
Press `Ctrl+Shift+B` to build using the default build task.

## Project Structure

```
flow/
├── src/              # Source files
│   ├── main.cpp      # UI and entry point
│   └── FlowEngine.cpp # Core automation engine
├── include/          # Header files
│   ├── FlowEngine.h
│   └── Protection.h
├── scripts/          # Build scripts
│   └── build.ps1
├── build/            # Build output (gitignored)
├── FLOW.ico          # Application icon
├── FLOW.manifest     # Windows manifest
└── resource.rc       # Resource definitions

```

## Usage

1. **Auto-Clicker Mode**
   - Press F6 to start/stop auto-clicking
   - Configure interval and loop count in settings

2. **Recording Mode**
   - Click "Record" button
   - Perform your actions (clicks and keystrokes)
   - Click "Stop All" to finish recording
   - Optionally save the recording

3. **Playback Mode**
   - Load a saved recording or use the current one
   - Click "Play" to execute
   - Press F8 to start/stop playback

## Hotkeys

- **F6** - Toggle Auto-Clicker
- **F8** - Toggle Playback

## License

This project is provided as-is for personal and educational use.

## Version

3.0.0 - Modern Professional Release
