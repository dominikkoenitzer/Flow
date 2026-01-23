/**
 * @file main_gui_v2.cpp
 * @brief FLOW Advanced GUI with Menu System & Customizable Hotkeys
 * 
 * @author FLOW Development Team
 * @version 2.0.0
 * @date 2026-01-21
 */

#include "FlowEngine.h"
#include "Protection.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace flow;

// ============================================================================
// CONFIGURATION
// ============================================================================

namespace FlowConfig {
    constexpr int DEFAULT_CLICK_INTERVAL_MS = 1;
    constexpr int MIN_CLICK_INTERVAL_MS = 1;
    constexpr int MAX_CLICK_INTERVAL_MS = 10000;
    constexpr double HUMANIZATION_MEAN = 0.0;
    constexpr double HUMANIZATION_STDDEV = 2.0;
    constexpr bool HUMANIZATION_DEFAULT_STATE = false;
}

// ============================================================================
// COLOR SCHEME
// ============================================================================

namespace Colors {
    const COLORREF Background = RGB(15, 15, 18);       // Deeper dark
    const COLORREF Surface = RGB(25, 27, 32);          // Card surface with blue tint
    const COLORREF SurfaceHover = RGB(32, 35, 42);     // Hover state
    const COLORREF Primary = RGB(88, 166, 255);        // Brighter blue
    const COLORREF Success = RGB(72, 219, 109);        // Vibrant green
    const COLORREF Warning = RGB(255, 179, 64);        // Warmer orange
    const COLORREF Danger = RGB(255, 89, 88);          // Bright red
    const COLORREF Text = RGB(245, 247, 250);          // Softer white
    const COLORREF TextSecondary = RGB(142, 152, 167); // Blue-gray
    const COLORREF Border = RGB(48, 52, 60);           // Subtle border
    const COLORREF Accent = RGB(138, 180, 255);        // Light blue accent
}

// ============================================================================
// CONTROL & MENU IDs
// ============================================================================

enum ControlID {
    // Buttons
    BTN_START_CLICKER = 101,
    BTN_STOP_CLICKER = 102,
    EDIT_CLICK_INTERVAL = 103,
    SLIDER_CLICK_INTERVAL = 104,
    BTN_START_RECORD = 201,
    BTN_STOP_RECORD = 202,
    BTN_CLEAR_RECORD = 203,
    BTN_PLAY_ONCE = 301,
    BTN_PLAY_LOOP = 302,
    BTN_STOP_PLAYBACK = 303,
    EDIT_LOOP_COUNT = 304,
    CHK_CONTINUOUS = 305,
    CHK_HUMANIZATION = 401,
    SLIDER_HUMANIZATION = 402,
    LABEL_HUMANIZATION_VALUE = 403,
    BTN_SAVE_MACRO = 501,
    BTN_LOAD_MACRO = 502,
    STATUS_TEXT = 601,
    TIMER_UPDATE = 1001,
    
    // Menu Items - File
    MENU_FILE = 1100,
    MENU_SAVE = 1101,
    MENU_LOAD = 1102,
    MENU_EXIT = 1103,
    
    // Menu Items - Playback
    MENU_PLAYBACK = 1200,
    MENU_SPEED_HALF = 1210,
    MENU_SPEED_1X = 1211,
    MENU_SPEED_2X = 1212,
    MENU_SPEED_CUSTOM = 1215,
    
    // Menu Items - Options
    MENU_OPTIONS = 1300,
    MENU_HOTKEY_CLICKER = 1301,
    MENU_HOTKEY_RECORD = 1302,
    MENU_HOTKEY_STOP_RECORD = 1303,
    MENU_HOTKEY_PLAYBACK = 1304,
    MENU_ALWAYS_ON_TOP = 1310,
    MENU_SHOW_STATUS = 1311,
    MENU_HUMANIZATION_SETTINGS = 1312,
    
    // Menu Items - Help
    MENU_HELP = 1400,
    MENU_ABOUT = 1401,
    
    // Hotkeys
    HOTKEY_TOGGLE_CLICKER = 2001,
    HOTKEY_TOGGLE_RECORD = 2002,
    HOTKEY_PLAYBACK = 2004
};

// ============================================================================
// HOTKEY CONFIGURATION
// ============================================================================

struct HotkeyConfig {
    int id;
    UINT modifiers;
    UINT vk;
    std::string name;
    std::string displayName;
};

std::map<int, HotkeyConfig> g_hotkeys = {
    {HOTKEY_TOGGLE_CLICKER, {HOTKEY_TOGGLE_CLICKER, 0, VK_F6, "Toggle Clicker", "F6"}},
    {HOTKEY_TOGGLE_RECORD, {HOTKEY_TOGGLE_RECORD, 0, VK_F8, "Toggle Recording", "F8"}},
    {HOTKEY_PLAYBACK, {HOTKEY_PLAYBACK, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'P', "Play/Stop", "Ctrl+Shift+Alt+P"}}
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

struct AppState {
    HWND hwnd = nullptr;
    HMENU hMenu = nullptr;
    FlowEngine* engine = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontNormal = nullptr;
    HFONT fontSmall = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushSurface = nullptr;
    int clickInterval = FlowConfig::DEFAULT_CLICK_INTERVAL_MS;
    float playbackSpeed = 1.0f;
    int loopCount = 1;
    bool alwaysOnTop = false;
    bool showStatus = true;
    bool continuousPlayback = false;
} g_app;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string VKToString(UINT vk) {
    switch (vk) {
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_PAUSE: return "Pause";
        case VK_SCROLL: return "Scroll Lock";
        case VK_PRINT: return "Print Screen";
        default: {
            char buf[32];
            sprintf(buf, "Key %d", vk);
            return buf;
        }
    }
}

void RegisterHotkeys(HWND hwnd) {
    for (auto& pair : g_hotkeys) {
        UnregisterHotKey(hwnd, pair.first);
        RegisterHotKey(hwnd, pair.first, pair.second.modifiers, pair.second.vk);
    }
}

void UpdateButtonLabels(HWND hwnd) {
    std::string recordBtnText = "RECORD (" + g_hotkeys[HOTKEY_TOGGLE_RECORD].displayName + ")";
    SetWindowTextA(GetDlgItem(hwnd, BTN_START_RECORD), recordBtnText.c_str());
    
    std::string playBtnText = "PLAY (" + g_hotkeys[HOTKEY_PLAYBACK].displayName + ")";
    SetWindowTextA(GetDlgItem(hwnd, BTN_PLAY_ONCE), playBtnText.c_str());
}

HFONT CreateModernFont(int size, int weight = FW_NORMAL) {
    // Get system DPI and scale font size accordingly
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    
    // Scale font size based on DPI (96 is baseline/100%)
    int scaledSize = -MulDiv(size, dpi, 72);
    
    // Try Segoe UI Variable Display first (Windows 11), fallback to Segoe UI
    HFONT font = CreateFont(scaledSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI Variable Display"));
    
    if (!font) {
        font = CreateFont(scaledSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
    }
    return font;
}

HWND CreateModernButton(HWND parent, const char* text, int x, int y, int width, int height, int id) {
    HWND btn = CreateWindowEx(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, width, height, parent, (HMENU)(UINT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessage(btn, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
    return btn;
}

HWND CreateModernEdit(HWND parent, const char* text, int x, int y, int width, int height, int id) {
    HWND edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        x, y, width, height, parent, (HMENU)(UINT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessage(edit, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
    return edit;
}

HWND CreateModernCheckbox(HWND parent, const char* text, int x, int y, int width, int height, int id, bool checked = false) {
    HWND chk = CreateWindowEx(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, width, height, parent, (HMENU)(UINT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessage(chk, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
    SendMessage(chk, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return chk;
}

HWND CreateModernSlider(HWND parent, int x, int y, int width, int height, int id, int min, int max, int pos) {
    HWND slider = CreateWindowEx(0, TRACKBAR_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        x, y, width, height, parent, (HMENU)(UINT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessage(slider, TBM_SETRANGE, TRUE, MAKELONG(min, max));
    SendMessage(slider, TBM_SETPOS, TRUE, pos);
    SendMessage(slider, TBM_SETTICFREQ, (max - min) / 10, 0);
    return slider;
}

void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    SelectObject(hdc, brush);
    SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawModernButton(LPDRAWITEMSTRUCT dis, const char* text, COLORREF color) {
    HDC hdc = dis->hDC;
    RECT rect = dis->rcItem;
    
    // Calculate button state colors
    COLORREF btnColor = color;
    COLORREF topColor = color;
    COLORREF bottomColor = color;
    
    if (dis->itemState & ODS_SELECTED) {
        // Pressed state - darker
        btnColor = RGB(GetRValue(color) * 0.6, GetGValue(color) * 0.6, GetBValue(color) * 0.6);
        topColor = bottomColor = btnColor;
    } else if (dis->itemState & ODS_FOCUS) {
        // Hover state - brighter with subtle gradient
        topColor = RGB(
            (GetRValue(color) * 1.2 > 255) ? 255 : (BYTE)(GetRValue(color) * 1.2),
            (GetGValue(color) * 1.2 > 255) ? 255 : (BYTE)(GetGValue(color) * 1.2),
            (GetBValue(color) * 1.2 > 255) ? 255 : (BYTE)(GetBValue(color) * 1.2)
        );
        bottomColor = RGB(
            (GetRValue(color) * 1.1 > 255) ? 255 : (BYTE)(GetRValue(color) * 1.1),
            (GetGValue(color) * 1.1 > 255) ? 255 : (BYTE)(GetGValue(color) * 1.1),
            (GetBValue(color) * 1.1 > 255) ? 255 : (BYTE)(GetBValue(color) * 1.1)
        );
    } else {
        // Normal state - subtle gradient
        topColor = RGB(
            (GetRValue(color) * 1.08 > 255) ? 255 : (BYTE)(GetRValue(color) * 1.08),
            (GetGValue(color) * 1.08 > 255) ? 255 : (BYTE)(GetGValue(color) * 1.08),
            (GetBValue(color) * 1.08 > 255) ? 255 : (BYTE)(GetBValue(color) * 1.08)
        );
        bottomColor = RGB(
            GetRValue(color) * 0.92,
            GetGValue(color) * 0.92,
            GetBValue(color) * 0.92
        );
    }
    
    // Draw gradient background
    int height = rect.bottom - rect.top;
    for (int i = 0; i < height; i++) {
        float ratio = (float)i / height;
        BYTE r = (BYTE)(GetRValue(topColor) * (1 - ratio) + GetRValue(bottomColor) * ratio);
        BYTE g = (BYTE)(GetGValue(topColor) * (1 - ratio) + GetGValue(bottomColor) * ratio);
        BYTE b = (BYTE)(GetBValue(topColor) * (1 - ratio) + GetBValue(bottomColor) * ratio);
        
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, rect.left, rect.top + i, NULL);
        LineTo(hdc, rect.right, rect.top + i);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
    
    // Draw rounded border
    HPEN borderPen = CreatePen(PS_SOLID, 1, Colors::Border);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    
    // Draw text with shadow for depth
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_app.fontNormal);
    
    // Text shadow
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 1, 1);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawText(hdc, text, -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    // Main text
    SetTextColor(hdc, Colors::Text);
    DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void UpdateStatusText() {
    if (!g_app.engine) return;
    
    std::stringstream ss;
    ss << "Status: ";
    
    if (g_app.engine->IsClickerActive()) {
        ss << "[CLICKER " << g_app.engine->GetClickInterval() << "ms] ";
    }
    if (g_app.engine->IsRecordingActive()) {
        ss << "[RECORDING " << g_app.engine->GetEventCount() << " events] ";
    }
    if (g_app.engine->IsPlaybackActive()) {
        int loopNum = g_app.engine->GetCurrentLoop();
        ss << "[PLAYING Loop #" << loopNum << "] ";
    }
    if (!g_app.engine->IsClickerActive() && !g_app.engine->IsRecordingActive() && !g_app.engine->IsPlaybackActive()) {
        ss << "Idle";
    }
    
    ss << " | Hotkeys: " << g_hotkeys[HOTKEY_TOGGLE_CLICKER].displayName << " (Clicker) "
       << g_hotkeys[HOTKEY_TOGGLE_RECORD].displayName << " (Record) "
       << g_hotkeys[HOTKEY_PLAYBACK].displayName << " (Play)";
    
    SetWindowTextA(GetDlgItem(g_app.hwnd, STATUS_TEXT), ss.str().c_str());
}

void UpdateMenuChecks() {
    // Playback speed checks
    CheckMenuItem(g_app.hMenu, MENU_SPEED_HALF, g_app.playbackSpeed == 0.5f ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(g_app.hMenu, MENU_SPEED_1X, g_app.playbackSpeed == 1.0f ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(g_app.hMenu, MENU_SPEED_2X, g_app.playbackSpeed == 2.0f ? MF_CHECKED : MF_UNCHECKED);
    
    // Options
    CheckMenuItem(g_app.hMenu, MENU_ALWAYS_ON_TOP, g_app.alwaysOnTop ? MF_CHECKED : MF_UNCHECKED);
}

// ============================================================================
// MENU CREATION
// ============================================================================

void CreateMenuBar(HWND hwnd) {
    g_app.hMenu = CreateMenu();
    
    // File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, MENU_SAVE, "Save Macro...\tCtrl+S");
    AppendMenu(hFileMenu, MF_STRING, MENU_LOAD, "Load Macro...\tCtrl+O");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, MENU_EXIT, "Exit\tAlt+F4");
    AppendMenu(g_app.hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");
    
    // Playback Menu
    // Playback Menu - Simple speed controls only
    HMENU hPlaybackMenu = CreatePopupMenu();
    AppendMenu(hPlaybackMenu, MF_STRING, MENU_SPEED_HALF, "0.5x Speed");
    AppendMenu(hPlaybackMenu, MF_STRING, MENU_SPEED_1X, "1x Speed");
    AppendMenu(hPlaybackMenu, MF_STRING, MENU_SPEED_2X, "2x Speed");
    AppendMenu(g_app.hMenu, MF_POPUP, (UINT_PTR)hPlaybackMenu, "Playback");
    
    // Options Menu
    HMENU hOptionsMenu = CreatePopupMenu();
    
    // Hotkeys submenu
    HMENU hHotkeyMenu = CreatePopupMenu();
    
    HMENU hHotkeyClicker = CreatePopupMenu();
    AppendMenu(hHotkeyClicker, MF_STRING, 5001, "F5");
    AppendMenu(hHotkeyClicker, MF_STRING, 5002, "F6");
    AppendMenu(hHotkeyClicker, MF_STRING, 5003, "F7");
    AppendMenu(hHotkeyClicker, MF_STRING, 5004, "Pause");
    AppendMenu(hHotkeyMenu, MF_POPUP, (UINT_PTR)hHotkeyClicker, "Auto-Clicker");
    
    HMENU hHotkeyRecord = CreatePopupMenu();
    AppendMenu(hHotkeyRecord, MF_STRING, 5011, "F7");
    AppendMenu(hHotkeyRecord, MF_STRING, 5012, "F8");
    AppendMenu(hHotkeyRecord, MF_STRING, 5013, "F9");
    AppendMenu(hHotkeyRecord, MF_STRING, 5014, "Print Screen");
    AppendMenu(hHotkeyMenu, MF_POPUP, (UINT_PTR)hHotkeyRecord, "Toggle Recording");
    
    HMENU hHotkeyPlayback = CreatePopupMenu();
    AppendMenu(hHotkeyPlayback, MF_STRING, 5031, "F8");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5032, "F11");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5033, "F12");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5034, "Pause");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5035, "Scroll Lock");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5036, "Print Screen");
    AppendMenu(hHotkeyPlayback, MF_STRING, 5037, "Ctrl+Shift+Alt+P");
    AppendMenu(hHotkeyMenu, MF_POPUP, (UINT_PTR)hHotkeyPlayback, "Playback");
    
    AppendMenu(hOptionsMenu, MF_POPUP, (UINT_PTR)hHotkeyMenu, "Hotkeys");
    AppendMenu(hOptionsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hOptionsMenu, MF_STRING, MENU_ALWAYS_ON_TOP, "Always on Top");
    AppendMenu(g_app.hMenu, MF_POPUP, (UINT_PTR)hOptionsMenu, "Options");
    
    // Help Menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenu(hHelpMenu, MF_STRING, MENU_ABOUT, "About FLOW...");
    AppendMenu(g_app.hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "Help");
    
    SetMenu(hwnd, g_app.hMenu);
    UpdateMenuChecks();
}

// ============================================================================
// CONTROL CREATION
// ============================================================================

void CreateControls(HWND hwnd) {
    int margin = 15;
    int groupY = 50;
    int groupSpacing = 140;
    int groupWidth = 320;
    int btnHeight = 32;
    int btnWidth = 155;
    int labelHeight = 22;
    
    // === AUTO-CLICKER SECTION ===
    int y = groupY;
    HWND labelClicker = CreateWindowEx(0, "STATIC", "AUTO-CLICKER", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, y, groupWidth, labelHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(labelClicker, WM_SETFONT, (WPARAM)g_app.fontTitle, TRUE);
    y += labelHeight + 15;
    
    CreateModernButton(hwnd, "START CLICKER", margin, y, btnWidth, btnHeight, BTN_START_CLICKER);
    CreateModernButton(hwnd, "STOP", margin + btnWidth + 10, y, btnWidth - 60, btnHeight, BTN_STOP_CLICKER);
    y += btnHeight + 15;
    
    CreateWindowEx(0, "STATIC", "Click Interval (ms):", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, y, 130, 22, hwnd, NULL, GetModuleHandle(NULL), NULL);
    CreateModernEdit(hwnd, "1", margin + 135, y - 2, 70, 28, EDIT_CLICK_INTERVAL);
    y += 35;
    CreateModernSlider(hwnd, margin, y, groupWidth - 10, 30, SLIDER_CLICK_INTERVAL, 1, 1000, 1);
    
    // === MACRO RECORDER SECTION ===
    y = groupY + groupSpacing;
    HWND labelRecorder = CreateWindowEx(0, "STATIC", "MACRO RECORDER", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, y, groupWidth, labelHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(labelRecorder, WM_SETFONT, (WPARAM)g_app.fontTitle, TRUE);
    y += labelHeight + 15;
    
    std::string recordBtnText = "RECORD (" + g_hotkeys[HOTKEY_TOGGLE_RECORD].displayName + ")";
    CreateModernButton(hwnd, recordBtnText.c_str(), margin, y, btnWidth, btnHeight, BTN_START_RECORD);
    CreateModernButton(hwnd, "CLEAR", margin + btnWidth + 10, y, btnWidth - 60, btnHeight, BTN_CLEAR_RECORD);
    
    // === PLAYBACK SECTION ===
    y = groupY + groupSpacing * 2;
    HWND labelPlayback = CreateWindowEx(0, "STATIC", "PLAYBACK", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, y, groupWidth, labelHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(labelPlayback, WM_SETFONT, (WPARAM)g_app.fontTitle, TRUE);
    y += labelHeight + 15;
    
    std::string playBtnText = "PLAY (" + g_hotkeys[HOTKEY_PLAYBACK].displayName + ")";
    CreateModernButton(hwnd, playBtnText.c_str(), margin, y, btnWidth, btnHeight, BTN_PLAY_ONCE);
    CreateModernButton(hwnd, "STOP", margin + btnWidth + 10, y, btnWidth - 60, btnHeight, BTN_STOP_PLAYBACK);
    
    // === HUMANIZATION (RIGHT COLUMN) ===
    y = groupY;
    int col2X = margin + groupWidth + 40;
    HWND labelHuman = CreateWindowEx(0, "STATIC", "HUMANIZATION", WS_CHILD | WS_VISIBLE | SS_LEFT,
        col2X, y, groupWidth, labelHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(labelHuman, WM_SETFONT, (WPARAM)g_app.fontTitle, TRUE);
    y += labelHeight + 15;
    
    CreateModernCheckbox(hwnd, "Enable Natural Timing Variation", col2X, y, 300, 32, CHK_HUMANIZATION, FlowConfig::HUMANIZATION_DEFAULT_STATE);
    y += 45;
    CreateWindowEx(0, "STATIC", "Randomness Level:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        col2X, y, 140, 22, hwnd, NULL, GetModuleHandle(NULL), NULL);
    char humanValBuf[32];
    sprintf(humanValBuf, "%.1f ms", FlowConfig::HUMANIZATION_STDDEV);
    CreateWindowEx(0, "STATIC", humanValBuf, WS_CHILD | WS_VISIBLE | SS_LEFT,
        col2X + 145, y, 80, 22, hwnd, (HMENU)(UINT_PTR)LABEL_HUMANIZATION_VALUE, GetModuleHandle(NULL), NULL);
    y += 30;
    CreateModernSlider(hwnd, col2X, y, groupWidth - 10, 30, SLIDER_HUMANIZATION, 0, 50, (int)(FlowConfig::HUMANIZATION_STDDEV * 10));
    
    // === FILE OPERATIONS ===
    y = groupY + groupSpacing;
    HWND labelFile = CreateWindowEx(0, "STATIC", "FILE OPERATIONS", WS_CHILD | WS_VISIBLE | SS_LEFT,
        col2X, y, groupWidth, labelHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(labelFile, WM_SETFONT, (WPARAM)g_app.fontTitle, TRUE);
    y += labelHeight + 15;
    
    CreateModernButton(hwnd, "SAVE MACRO", col2X, y, btnWidth, btnHeight, BTN_SAVE_MACRO);
    CreateModernButton(hwnd, "LOAD MACRO", col2X + btnWidth + 10, y, btnWidth, btnHeight, BTN_LOAD_MACRO);
    
    // === STATUS BAR ===
    HWND status = CreateWindowEx(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, 450, 670, 30, hwnd, (HMENU)STATUS_TEXT, GetModuleHandle(NULL), NULL);
    SendMessage(status, WM_SETFONT, (WPARAM)g_app.fontSmall, TRUE);
    
    // Set fonts for static labels
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
        char className[256];
        GetClassNameA(child, className, sizeof(className));
        if (strcmp(className, "Static") == 0 && GetDlgCtrlID(child) != STATUS_TEXT) {
            SendMessage(child, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
        }
        return TRUE;
    }, 0);
}

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

void HandleCommand(HWND hwnd, WPARAM wParam) {
    int id = LOWORD(wParam);
    
    switch (id) {
        // Button commands
        case BTN_START_CLICKER: {
            char buf[32];
            GetWindowTextA(GetDlgItem(hwnd, EDIT_CLICK_INTERVAL), buf, sizeof(buf));
            int interval = atoi(buf);
            if (interval < 1) interval = 1;
            if (interval > 10000) interval = 10000;
            g_app.clickInterval = interval;
            g_app.engine->StartAutoClicker(interval);
            break;
        }
        
        case BTN_STOP_CLICKER:
            g_app.engine->StopAutoClicker();
            break;
        
        case BTN_START_RECORD:
            if (g_app.engine->IsRecordingActive()) {
                g_app.engine->StopRecording();
            } else {
                g_app.engine->StartRecording();
            }
            break;
        
        case BTN_CLEAR_RECORD:
            g_app.engine->ClearRecording();
            break;
        
        case BTN_PLAY_ONCE:
            // Stop recording before starting playback
            if (g_app.engine->IsRecordingActive()) {
                g_app.engine->StopRecording();
            }
            // Always start continuous playback (loops until stopped)
            g_app.engine->StartPlayback(-1);
            break;
        
        case BTN_STOP_PLAYBACK:
            g_app.engine->StopPlayback();
            break;
        
        case CHK_HUMANIZATION: {
            bool enabled = SendMessage(GetDlgItem(hwnd, CHK_HUMANIZATION), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_app.engine->EnableHumanization(enabled);
            break;
        }
        
        // File menu
        case BTN_SAVE_MACRO:
        case MENU_SAVE: {
            OPENFILENAMEW ofn = {0};
            wchar_t fileName[MAX_PATH] = L"macro.flow";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"FLOW Macros (*.flow)\0*.flow\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"flow";
            
            if (GetSaveFileNameW(&ofn)) {
                if (g_app.engine->SaveMacro(fileName)) {
                    MessageBoxA(hwnd, "Macro saved successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hwnd, "Failed to save macro.", "Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }
        
        case BTN_LOAD_MACRO:
        case MENU_LOAD: {
            OPENFILENAMEW ofn = {0};
            wchar_t fileName[MAX_PATH] = L"";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"FLOW Macros (*.flow)\0*.flow\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            
            if (GetOpenFileNameW(&ofn)) {
                if (g_app.engine->LoadMacro(fileName)) {
                    MessageBoxA(hwnd, "Macro loaded successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hwnd, "Failed to load macro.", "Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }
        
        case MENU_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        
        // Playback speed
        case MENU_SPEED_HALF: g_app.playbackSpeed = 0.5f; UpdateMenuChecks(); break;
        case MENU_SPEED_1X: g_app.playbackSpeed = 1.0f; UpdateMenuChecks(); break;
        case MENU_SPEED_2X: g_app.playbackSpeed = 2.0f; UpdateMenuChecks(); break;
        
        // Options
        case MENU_ALWAYS_ON_TOP:
            g_app.alwaysOnTop = !g_app.alwaysOnTop;
            SetWindowPos(hwnd, g_app.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            UpdateMenuChecks();
            break;
        
        case MENU_SHOW_STATUS:
            g_app.showStatus = !g_app.showStatus;
            ShowWindow(GetDlgItem(hwnd, STATUS_TEXT), g_app.showStatus ? SW_SHOW : SW_HIDE);
            UpdateMenuChecks();
            break;
        
        case MENU_ABOUT:
            MessageBoxA(hwnd,
                "FLOW - Input Automation Engine\n\n"
                "QUICK START:\n"
                "1. Auto-Clicker: Set interval → START CLICKER (F6 to toggle)\n"
                "2. Record Macro: Press F8 → Perform actions → Press F8 to stop\n"
                "3. Playback: Press Ctrl+Shift+Alt+P or click PLAY\n\n"
                "HOTKEYS (customizable in Options → Hotkeys):\n"
                "• F6 - Toggle auto-clicker\n"
                "• F8 - Toggle macro recording\n"
                "• Ctrl+Shift+Alt+P - Play/Stop macro\n\n"
                "FEATURES:\n"
                "- High-speed clicking (1-10000ms intervals)\n"
                "- Precise macro recording & playback\n"
                "- Loop control (single, multiple, continuous)\n"
                "- Playback speed adjustment (0.5x - 2x)\n"
                "- Optional humanization for natural timing\n\n"
                "⚠️ Run as Administrator for full functionality\n\n"
                "Copyright © 2026 FLOW Project",
                "About FLOW", MB_OK | MB_ICONINFORMATION);
            break;
        
        // Hotkey customization
        case 5001: case 5002: case 5003: case 5004: {
            UINT vk = VK_F6;
            if (id == 5001) vk = VK_F5;
            else if (id == 5002) vk = VK_F6;
            else if (id == 5003) vk = VK_F7;
            else if (id == 5004) vk = VK_PAUSE;
            g_hotkeys[HOTKEY_TOGGLE_CLICKER].vk = vk;
            g_hotkeys[HOTKEY_TOGGLE_CLICKER].displayName = VKToString(vk);
            RegisterHotkeys(hwnd);
            break;
        }
        
        case 5011: case 5012: case 5013: case 5014: {
            UINT vk = VK_F8;
            if (id == 5011) vk = VK_F7;
            else if (id == 5012) vk = VK_F8;
            else if (id == 5013) vk = VK_F9;
            else if (id == 5014) vk = VK_PRINT;
            g_hotkeys[HOTKEY_TOGGLE_RECORD].vk = vk;
            g_hotkeys[HOTKEY_TOGGLE_RECORD].displayName = VKToString(vk);
            RegisterHotkeys(hwnd);
            UpdateButtonLabels(hwnd);
            break;
        }
        
        case 5031: case 5032: case 5033: case 5034: case 5035: case 5036: case 5037: {
            UINT vk = VK_F12;
            UINT modifiers = 0;
            if (id == 5031) vk = VK_F8;
            else if (id == 5032) vk = VK_F11;
            else if (id == 5033) vk = VK_F12;
            else if (id == 5034) vk = VK_PAUSE;
            else if (id == 5035) vk = VK_SCROLL;
            else if (id == 5036) vk = VK_PRINT;
            else if (id == 5037) {
                vk = 'P';
                modifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
            }
            g_hotkeys[HOTKEY_PLAYBACK].vk = vk;
            g_hotkeys[HOTKEY_PLAYBACK].modifiers = modifiers;
            if (modifiers != 0) {
                g_hotkeys[HOTKEY_PLAYBACK].displayName = "Ctrl+Shift+Alt+P";
            } else {
                g_hotkeys[HOTKEY_PLAYBACK].displayName = VKToString(vk);
            }
            RegisterHotkeys(hwnd);
            UpdateButtonLabels(hwnd);
            break;
        }
    }
}

void HandleHScroll(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    HWND slider = (HWND)lParam;
    int pos = SendMessage(slider, TBM_GETPOS, 0, 0);
    int id = GetDlgCtrlID(slider);
    
    if (id == SLIDER_CLICK_INTERVAL) {
        char buf[32];
        sprintf(buf, "%d", pos);
        SetWindowTextA(GetDlgItem(hwnd, EDIT_CLICK_INTERVAL), buf);
        g_app.clickInterval = pos;
    } else if (id == SLIDER_HUMANIZATION) {
        double stddev = pos / 10.0;
        g_app.engine->ConfigureHumanization(FlowConfig::HUMANIZATION_MEAN, stddev);
        char buf[32];
        sprintf(buf, "%.1f ms", stddev);
        SetWindowTextA(GetDlgItem(hwnd, LABEL_HUMANIZATION_VALUE), buf);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            CreateMenuBar(hwnd);
            SetTimer(hwnd, TIMER_UPDATE, 100, NULL);
            RegisterHotkeys(hwnd);
            break;
        
        case WM_SIZE: {
            // Get new window dimensions
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            
            // Calculate scaling factors (base size was 900x700)
            float scaleX = width / 900.0f;
            float scaleY = height / 700.0f;
            
            // Reposition all child controls
            struct ResizeData {
                float scaleX;
                float scaleY;
            };
            ResizeData data = {scaleX, scaleY};
            
            EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
                ResizeData* data = (ResizeData*)lParam;
                
                // Get control ID to identify static original positions
                int id = GetDlgCtrlID(child);
                static std::map<HWND, RECT> originalPositions;
                
                RECT rect;
                GetWindowRect(child, &rect);
                MapWindowPoints(HWND_DESKTOP, GetParent(child), (LPPOINT)&rect, 2);
                
                // Store original position on first call
                if (originalPositions.find(child) == originalPositions.end()) {
                    originalPositions[child] = rect;
                    return TRUE;
                }
                
                RECT& orig = originalPositions[child];
                
                // Scale position and size
                int newX = (int)(orig.left * data->scaleX);
                int newY = (int)(orig.top * data->scaleY);
                int newW = (int)((orig.right - orig.left) * data->scaleX);
                int newH = (int)((orig.bottom - orig.top) * data->scaleY);
                
                SetWindowPos(child, NULL, newX, newY, newW, newH, 
                           SWP_NOZORDER | SWP_NOACTIVATE);
                
                return TRUE;
            }, (LPARAM)&data);
            
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        
        case WM_HOTKEY: {
            int hotkeyId = wParam;
            switch (hotkeyId) {
                case HOTKEY_TOGGLE_CLICKER:
                    if (g_app.engine->IsClickerActive()) {
                        g_app.engine->StopAutoClicker();
                    } else {
                        g_app.engine->StartAutoClicker(g_app.clickInterval);
                    }
                    break;
                
                case HOTKEY_TOGGLE_RECORD:
                    if (g_app.engine->IsRecordingActive()) {
                        g_app.engine->StopRecording();
                    } else {
                        g_app.engine->StartRecording();
                    }
                    break;
                
                case HOTKEY_PLAYBACK:
                    if (g_app.engine->IsPlaybackActive()) {
                        g_app.engine->StopPlayback();
                    } else {
                        // Stop recording before starting playback
                        if (g_app.engine->IsRecordingActive()) {
                            g_app.engine->StopRecording();
                        }
                        // Hotkey always starts continuous playback (loops forever until stopped)
                        g_app.engine->StartPlayback(-1);
                    }
                    break;
            }
            break;
        }
        
        case WM_TIMER:
            if (wParam == TIMER_UPDATE) {
                UpdateStatusText();
            }
            break;
        
        case WM_COMMAND:
            HandleCommand(hwnd, wParam);
            break;
        
        case WM_HSCROLL:
            HandleHScroll(hwnd, wParam, lParam);
            break;
        
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            int id = dis->CtlID;
            char text[256];
            GetWindowTextA(dis->hwndItem, text, sizeof(text));
            
            COLORREF color = Colors::Primary;
            if (id == BTN_START_CLICKER) color = Colors::Success;
            else if (id == BTN_START_RECORD) color = g_app.engine->IsRecordingActive() ? Colors::Danger : Colors::Success;
            else if (id == BTN_STOP_CLICKER || id == BTN_STOP_PLAYBACK) color = Colors::Danger;
            else if (id == BTN_PLAY_LOOP) color = Colors::Warning;
            else if (id == BTN_CLEAR_RECORD) color = Colors::TextSecondary;
            
            DrawModernButton(dis, text, color);
            return TRUE;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, Colors::Text);
            SetBkColor(hdcStatic, Colors::Background);
            return (LRESULT)g_app.brushBackground;
        }
        
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, Colors::Text);
            SetBkColor(hdcEdit, Colors::Surface);
            return (LRESULT)g_app.brushSurface;
        }
        
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, g_app.brushBackground);
            return 1;
        }
        
        case WM_CLOSE:
            if (MessageBoxA(hwnd, "Are you sure you want to exit FLOW?", "Exit", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                DestroyWindow(hwnd);
            }
            break;
        
        case WM_DPICHANGED: {
            // Handle DPI change for per-monitor scaling
            UINT newDpi = HIWORD(wParam);
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hwnd,
                NULL,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            
            // Recreate fonts at new DPI
            DeleteObject(g_app.fontTitle);
            DeleteObject(g_app.fontNormal);
            DeleteObject(g_app.fontSmall);
            g_app.fontTitle = CreateModernFont(20, FW_SEMIBOLD);
            g_app.fontNormal = CreateModernFont(15, FW_NORMAL);
            g_app.fontSmall = CreateModernFont(13, FW_NORMAL);
            
            // Update all child window fonts
            EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
                int id = GetDlgCtrlID(child);
                if (id >= 100 && id < 600) {
                    SendMessage(child, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
                }
                return TRUE;
            }, 0);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_UPDATE);
            for (auto& pair : g_hotkeys) {
                UnregisterHotKey(hwnd, pair.first);
            }
            PostQuitMessage(0);
            break;
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// ENTRY POINT
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Anti-debugging protection
    flow::protection::EnforceProtection();
    
    // Enable DPI Awareness for crisp rendering on high-DPI displays
    SetProcessDPIAware();
    
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    g_app.fontTitle = CreateModernFont(20, FW_SEMIBOLD);
    g_app.fontNormal = CreateModernFont(15, FW_NORMAL);
    g_app.fontSmall = CreateModernFont(13, FW_NORMAL);
    g_app.brushBackground = CreateSolidBrush(Colors::Background);
    g_app.brushSurface = CreateSolidBrush(Colors::Surface);
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_app.brushBackground;
    wc.lpszClassName = "FlowMainWindow";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        MessageBoxA(NULL, "Window registration failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    int width = 700;
    int height = 500;
    g_app.hwnd = CreateWindowEx(0, "FlowMainWindow", "FLOW - Input Automation Engine",
        WS_OVERLAPPEDWINDOW,
        (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
        width, height, NULL, NULL, hInstance, NULL);
    
    if (!g_app.hwnd) {
        MessageBoxA(NULL, "Window creation failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    g_app.engine = new FlowEngine();
    g_app.engine->ConfigureHumanization(FlowConfig::HUMANIZATION_MEAN, FlowConfig::HUMANIZATION_STDDEV);
    g_app.engine->EnableHumanization(FlowConfig::HUMANIZATION_DEFAULT_STATE);
    
    if (!g_app.engine->InstallHooks()) {
        MessageBoxA(NULL, "Failed to install input hooks!\n\nPlease run FLOW as Administrator.",
            "Error", MB_OK | MB_ICONERROR);
        delete g_app.engine;
        return 1;
    }
    
    CreateControls(g_app.hwnd);
    ShowWindow(g_app.hwnd, nCmdShow);
    UpdateWindow(g_app.hwnd);
    UpdateStatusText();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    g_app.engine->UninstallHooks();
    delete g_app.engine;
    DeleteObject(g_app.fontTitle);
    DeleteObject(g_app.fontNormal);
    DeleteObject(g_app.fontSmall);
    DeleteObject(g_app.brushBackground);
    DeleteObject(g_app.brushSurface);
    
    return (int)msg.wParam;
}
