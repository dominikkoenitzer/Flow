/**
 * @file AppState.h
 * @brief The window's control IDs, cached fonts, and the single app state.
 *
 * FLOW is one window with one document, so the state is one global rather than
 * a model threaded through every handler. Declaring it here — instead of at the
 * top of main.cpp — is what lets the settings, drawing and dialog code live in
 * their own translation units.
 */
#pragma once

#include "FlowEngine.h"

#include <windows.h>
#include <string>

namespace flow::ui {

enum ControlID {
    BTN_OPEN = 101,
    BTN_SAVE = 102,
    BTN_RECORD = 103,
    BTN_PLAY = 104,
    BTN_STOP_ALL = 105,
    BTN_SETTINGS = 106,
    BTN_TOGGLE_CLICKER = 107,

    EDIT_SPEED = 120,
    EDIT_LOOPS = 121,
    EDIT_INTERVAL = 122,
    CHK_CONTINUOUS = 123,
    CHK_HUMANIZE = 124,

    MENU_ALWAYS_ON_TOP = 211,
    MENU_CLEAR_RECORDING = 212,
    MENU_ABOUT = 213,
    MENU_CUSTOMIZE_HOTKEYS = 214,
    MENU_REOPEN_LAST = 215,

    HOTKEY_RECORD = 301,
    HOTKEY_PLAYBACK = 302,
    HOTKEY_CLICKER = 303,
    HOTKEY_STOP = 304,

    IDC_HOTKEY_RECORD = 401,
    IDC_HOTKEY_PLAYBACK = 402,
    IDC_HOTKEY_CLICKER = 403,
    IDC_HOTKEY_STOP = 404,
    IDC_HOTKEY_OK = 405,
    IDC_HOTKEY_CANCEL = 406,

    TIMER_STATUS_CHECK = 500,
};

/** Cached UI fonts (created in WinMain, destroyed at exit). */
struct UiFonts {
    HFONT wordmark = nullptr;  // bold brand
    HFONT cardTitle = nullptr; // small uppercase section label
    HFONT button = nullptr;    // button labels
    HFONT body = nullptr;      // normal text
    HFONT value = nullptr;     // edit-field values
    HFONT pill = nullptr;      // status label
    HFONT small_ = nullptr;    // muted captions
    HFONT mono = nullptr;      // monospace numerics
};
extern UiFonts g_fonts;

struct AppState {
    HWND hwnd = nullptr;
    FlowEngine* engine = nullptr;
    float playbackSpeed = 1.0f;
    int clickInterval = 100;
    int loopCount = 1;
    bool continuous = false;
    bool alwaysOnTop = false;
    bool isRecording = false;
    bool isPlaying = false;
    bool isClicking = false;
    bool humanizationEnabled = false;
    double humanizationStdDev = 2.0;
    
    // Hotkey settings
    UINT hotkeyRecord = VK_F8;
    UINT hotkeyPlayback = VK_F9;
    UINT hotkeyClicker = VK_F6;
    UINT hotkeyStop = VK_PAUSE;
    UINT hotkeyModifiers = 0;  // optional modifiers for playback (none by default)

    // Quality-of-life persistence
    bool reopenLastMacro = false;   // reload the last macro on startup
    std::wstring lastMacroPath;     // last opened / saved / dropped .rec file
    bool hasWinPos = false;         // whether a saved window position exists
    int winX = 0, winY = 0;         // last window top-left (restored on launch)
};
extern AppState g_app;

// Scratch copies the hotkey dialog edits, committed to g_app only on OK.
extern UINT g_tempHotkeyRecord;
extern UINT g_tempHotkeyPlayback;
extern UINT g_tempHotkeyClicker;
extern UINT g_tempHotkeyStop;

extern HWND g_hHotkeyRecordEdit;
extern HWND g_hHotkeyPlaybackEdit;
extern HWND g_hHotkeyClickerEdit;
extern HWND g_hHotkeyStopEdit;

}  // namespace flow::ui
