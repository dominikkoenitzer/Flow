# FLOW - Input Automation Engine

<div align="center">

**Flexible Low-latency Operations Workflow**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)

Professional-grade input automation for Windows with high-speed auto-clicking, macro recording/playback, and intelligent humanization.

</div>

---

## 🚀 Features

### ⚡ High-Speed Auto-Clicker
- **1ms minimum interval** - Up to 1000 clicks per second
- **Configurable intervals** - 1-10000ms range
- **SendInput API** - Maximum compatibility
- **Real-time control** - Adjust speed on-the-fly

### 🎬 Macro Recorder & Player
- **Global input capture** - Mouse and keyboard
- **Millisecond precision** - Accurate timing
- **Loop controls** - Single, multiple, or continuous playback
- **Save/Load macros** - Reusable automation sequences

### 🎭 Humanization Engine
- **Natural variance** - Gaussian distribution timing
- **Pattern bypass** - Evades basic detection
- **Configurable** - Adjust randomness level
- **Optional** - Toggle on/off as needed

### 🎨 Modern GUI
- **DPI-aware** - Crisp on all displays
- **Dark theme** - Professional appearance
- **Global hotkeys** - F8 recording, customizable playback
- **Status feedback** - Real-time operation display

## 📦 Installation

### Option 1: Pre-built Binary (Recommended)

1. Download `FLOW.exe` from the [latest release](../../releases)
2. Run as Administrator (required for global hooks)
3. Start automating!

### Option 2: Build from Source

**Requirements:**
- Windows 10/11
- MinGW-w64 (g++) or Visual Studio 2019+
- Git

**Build Steps:**

```powershell
# Clone the repository
git clone https://github.com/yourusername/flow.git
cd flow

# Build using VS Code task (Ctrl+Shift+B)
# Or use command line:
g++ -std=c++17 -O3 -mwindows -Iinclude -o build/FLOW.exe src/main_gui_v2.cpp src/FlowEngine.cpp -luser32 -lgdi32 -lcomctl32 -static-libgcc -static-libstdc++

# Run as Administrator
.\build\FLOW.exe
```

## 🎯 Usage

### Quick Start

1. **Launch FLOW** as Administrator
2. **Auto-Clicker**: Set interval → Click "START CLICKER"
3. **Record Macro**: Press F8 → Perform actions → Press F8 again to stop
4. **Playback**: Set loop count → Click "PLAY ONCE" or "PLAY LOOP"

### Hotkeys

| Key | Function |
|-----|----------|
| **F8** | Toggle macro recording |
| **Ctrl+Shift+Alt+P** | Start macro playback |
| **ESC** | Stop any running operation |

### Auto-Clicker

- Adjust interval using slider or text input (1-10000ms)
- **Humanization** adds natural variance (disable for precise timing)
- Click START/STOP or use menu hotkeys

### Macro Recording

- Press F8 to start recording
- Perform mouse movements, clicks, and keyboard actions
- Press F8 again to stop
- Save macro via File → Save

### Macro Playback

- Load saved macro or use recorded actions
- Set loop count (1-10000) or check **Continuous** for infinite loop
- Adjust playback speed: 0.5x, 1x, 2x via Playback menu
- Click PLAY LOOP to start

## 🛠️ Technical Details

### Architecture

```
FLOW/
├── src/
│   ├── FlowEngine.cpp        # Core automation engine
│   └── main_gui_v2.cpp        # GUI implementation
├── include/
│   └── FlowEngine.h           # Engine API
├── build/
│   └── FLOW.exe              # Compiled binary
└── scripts/
    └── build.ps1             # Build automation
```

### Key Technologies

- **C++17** - Modern C++ features
- **Win32 API** - Native Windows GUI
- **Low-level hooks** - WH_MOUSE_LL, WH_KEYBOARD_LL
- **SendInput API** - Input simulation
- **Multi-threading** - std::thread, std::atomic
- **DPI awareness** - High-resolution display support

## ⚠️ Important Notes

### Administrator Privileges Required
FLOW uses low-level input hooks which require administrator privileges on Windows. Always run as Administrator.

### Antivirus Warning
Some antivirus software may flag FLOW due to its use of input hooks and SendInput API. This is a false positive - FLOW is open-source and safe. Add an exception if needed.

### Responsible Use
FLOW is designed for legitimate automation tasks. Do not use it to:
- Violate terms of service
- Gain unfair advantages in online games
- Automate actions against a system's rules

## 📄 License

MIT License - See [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📞 Support

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)

## 🙏 Acknowledgments

Built with modern C++ and the Win32 API.

---

<div align="center">

Made with ⚡ by the FLOW team

</div>
