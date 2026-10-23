---
name: flow
description: Explains how to use Flow, a Windows tool that records mouse and keyboard input and plays it back, with a separate auto-clicker. Use it when the user asks how to record, replay, loop, speed up or stop a Flow recording, how to set the auto-clicker or the hotkeys, where Flow keeps its settings, or how to read or edit a saved .rec file.
---

# Flow

Flow records what you do with the mouse and keyboard and plays it back. It also has an auto-clicker that clicks at a fixed interval. It is one file, `FLOW.exe`, for 64-bit Windows 10 and 11. Its window title reads `FLOW`.

Flow has no scripting interface and no network access. Its only command line is `FLOW.exe --play "macro.rec"`, which plays one saved recording (see Playing from a shortcut). Help the user by explaining the steps, by reading or preparing its settings file, and by inspecting or editing saved recordings when they ask.

## Install, update, remove

- Install: download `FLOW.exe` and double-click it. Nothing gets installed. If Windows says it protected the PC, click More info, then Run anyway.
- Flow always starts as administrator and shows a UAC prompt. The exe asks for it in its manifest. Windows does not let a normal program record input while an elevated window has focus, or send input into one (Task Manager, an elevated terminal, most installers), so elevation lets Flow work in every window.
- Update: close Flow, download `FLOW.exe` again and replace the old file. Flow never updates itself.
- Remove: close Flow, then delete `FLOW.exe` and the folder `%APPDATA%\FLOW`.

## Hotkeys

The hotkeys work in any window. Defaults:

| Action | Key |
|---|---|
| Start or stop recording | F8 |
| Start or stop playback | F9 |
| Start or stop the auto-clicker | F6 |
| Stop everything (recording, playback and clicker) | Pause |

To change them: Settings, then Customize Hotkeys, click a field and press the new key. The fields take F1 to F12, A to Z and 0 to 9, without Ctrl, Alt or Shift, and the four keys must differ. Flow leaves its own hotkeys out of a recording, and clicks on Flow's own window are not recorded either.

## Recording and playback

1. Press F8 (or the Record button) to start. Starting a new recording wipes the one in memory, so save it first if the user wants to keep it.
2. Do the task, then press F8 again to stop.
3. Press F9 to play it back. Press F9 again or Pause to stop at any time, also during a long pause inside the recording.

Playback options in the window:
- Speed: a multiplier from 0.1 to 100. 2 plays twice as fast, 0.5 at half speed.
- Loops: how many times to play it, 1 to 999.
- Loop continuously: repeat until stopped.
- Humanize timing: adds small random variation to each delay of 1 ms or longer, so a run is not an exact repeat. The spread is set by `humanizationStdDev` in the settings file (default 2 ms); the window has no field for it.

Mouse positions are stored in screen pixels and replayed at the same absolute positions across all monitors. A recording made with a different monitor layout, resolution or scaling, or with windows in other places, clicks in the wrong spots.

Recorded input: mouse moves, left, right and middle clicks, the two side buttons, vertical and horizontal wheel, and key presses with their timing in microseconds.

## Saving and opening

- Save writes the recording to a `.rec` file through a save dialog. Open loads one. Dropping a `.rec` file onto the window loads it too.
- Settings, then Reopen Last Macro on Launch: Flow loads the last opened, saved or dropped file at the next start.
- Settings, then Clear Recording empties the recording in memory.
- A file Flow cannot read shows "That file isn't a valid FLOW macro (.rec)." and the recording in memory stays as it was.

The `.rec` format is binary. Read [references/rec-format.md](references/rec-format.md) before inspecting or changing one.

## Trimming and pauses

Settings, then Trim & Timing, edits the recording in memory. It is greyed out while recording or playing, and when the recording is empty.
- Start at and End at (ms): keep only the events in that span, counted from the start of the recording. Leave a field empty to keep everything on that side. The end is inclusive.
- Shorten pauses over (ms): every wait longer than this becomes this long, the wait before the first event too. Leave it empty to keep the timing.
- The line under the fields says how many events and seconds the result keeps. Apply replaces the recording in memory; Cancel leaves it as it was. Save afterwards to keep the result in a file.
- A cut start moves the cursor to where it was and presses again any key or button held at that moment. Keys and buttons the cut end leaves held are released at the end, so a trimmed recording never leaves a key stuck down.

## Playing from a shortcut

- `FLOW.exe --play "C:\path\macro.rec"` opens that file and plays it with the speed, loop and humanize settings from the settings file, then closes when the run ends. It starts minimised and does not take the focus. Pause stops it as usual.
- If Flow is already open, the open window plays the file instead and stays open. It refuses while it is recording, playing, or showing a dialog, and says so.
- Settings, then Create Play Shortcut, saves such a shortcut (`.lnk`) for the last opened or saved macro, on the desktop by default. It is greyed out until a macro has been opened or saved.
- Flow runs as administrator, so every launch from a shortcut shows the UAC prompt. A program that is not elevated cannot hand a file to an open Flow window; Windows blocks it.
- A launch with `--play` and no file shows a usage message. A file Flow cannot read shows the invalid-macro message, and Flow does not start.

## Auto-clicker

Set it in the window, then press F6 or its button:
- Interval (ms): 1 to 10000, default 100.
- Jitter (ms): a random amount added to or taken off each interval, 0 to 10000.
- Button: Left, Right or Middle.
- Clicks: single, double or triple click per action.
- Stop after: number of actions before it stops; 0 runs until stopped.
- Target: Cursor clicks wherever the mouse is. Point clicks at a fixed spot; drag from the Target control to pick it.

## Settings file

Flow keeps its settings in `%APPDATA%\FLOW\settings.cfg`, a plain `key=value` text file. See [references/settings.md](references/settings.md) for every key. Flow reads the file at start. It writes it right after each change in the window (a field when it loses focus, a toggle, a hotkey, an opened, saved or dropped macro) and on close, and only when the settings differ from what the file last held. So close Flow before editing the file, or the next change in the window, or closing Flow after moving the window, overwrites your edit.

## Safety rules for you

- A recording sends real mouse and keyboard input to whatever window is in front. Before the user plays a recording, tell them Pause stops everything, and suggest a first run with Loops at 1 in a harmless window.
- Warn that a looped or continuous run keeps clicking and typing until stopped, and that recordings replay as administrator, so they reach elevated windows too.
- Never edit a `.rec` or `settings.cfg` file the user did not ask you to change. Copy the file first and edit the copy or keep the original as a backup.
- Remind the user that the auto-clicker and playback may break the rules of games or online services; checking those rules is up to them.
