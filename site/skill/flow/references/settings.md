# settings.cfg

Path: `%APPDATA%\FLOW\settings.cfg`. One `key=value` per line. Flow reads it at start and writes it right after each change in the window and on close, but only when the settings differ from what the file last held; close Flow before editing. Each write goes to a temporary file beside it first, which then replaces the file. Unknown keys and lines without `=` are ignored. A value outside its range is ignored and the default stays.

| Key | Values | Default | Meaning |
|---|---|---|---|
| `playbackSpeed` | 0.1 to 100 | 1 | playback speed multiplier |
| `loopCount` | 1 to 999 | 1 | how many times playback runs |
| `continuous` | 0 or 1 | 0 | repeat playback until stopped |
| `humanization` | 0 or 1 | 0 | random variation on playback delays |
| `humanizationStdDev` | 0 to 1000 | 2 | spread of that variation, in milliseconds |
| `clickInterval` | 1 to 10000 | 100 | auto-clicker interval in milliseconds |
| `clickJitter` | 0 to 10000 | 0 | random milliseconds added to or taken off each interval |
| `clickButton` | 0, 1, 2 | 0 | left, right, middle |
| `clickCount` | 1, 2, 3 | 1 | clicks per action |
| `clickLimit` | 0 to 999999 | 0 | actions before the clicker stops, 0 means until stopped |
| `clickAtPoint` | 0 or 1 | 0 | click at the picked point instead of the cursor |
| `clickX`, `clickY` | integers | none | the picked point, in screen pixels |
| `hotkeyRecord` | virtual key code | 119 (F8) | record hotkey |
| `hotkeyPlayback` | virtual key code | 120 (F9) | playback hotkey |
| `hotkeyClicker` | virtual key code | 117 (F6) | auto-clicker hotkey |
| `hotkeyStop` | virtual key code | 19 (Pause) | stop everything |
| `alwaysOnTop` | 0 or 1 | 0 | keep the window above others |
| `reopenLastMacro` | 0 or 1 | 0 | load the last macro at start |
| `lastMacro` | file path, UTF-8 | none | last opened, saved or dropped .rec file |
| `winX`, `winY` | integers | none | window position |

Hotkey values are decimal Windows virtual key codes, for example 112 to 123 for F1 to F12. Changing them in Settings, then Customize Hotkeys, is safer than editing the file.

To reset Flow to its defaults, close it and delete `settings.cfg`.
