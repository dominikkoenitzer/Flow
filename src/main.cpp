/**
 * @file main.cpp
 * @brief FLOW - Modern Professional UI Design
 * @author FLOW Development Team
 * @version 3.0.0
 */

#include "FlowEngine.h"
#include "Protection.h"
#include <windows.h>
#include <commctrl.h>
#include <shellscalingapi.h>
#include <string>
#include <sstream>
#include <cmath>

using namespace flow;

// ============================================================================
// MODERN DESIGN SYSTEM
// ============================================================================

// Layout Constants
constexpr int BUTTON_SIZE = 80;
constexpr int BUTTON_SPACING = 12;
constexpr int MARGIN = 20;
constexpr int TOOLBAR_HEIGHT = BUTTON_SIZE + MARGIN * 2;
constexpr int STATUS_HEIGHT = 100;
constexpr int WINDOW_FULL_HEIGHT = TOOLBAR_HEIGHT + STATUS_HEIGHT;
constexpr int CORNER_RADIUS = 8;

// Modern Professional Color Palette
const COLORREF BG_PRIMARY = RGB(248, 249, 252);           // Light background
const COLORREF BG_ELEVATED = RGB(255, 255, 255);          // White cards
const COLORREF ACCENT_PRIMARY = RGB(59, 130, 246);        // Modern blue
const COLORREF ACCENT_HOVER = RGB(96, 165, 250);          // Lighter blue
const COLORREF ACCENT_PRESSED = RGB(37, 99, 235);         // Darker blue
const COLORREF SUCCESS_COLOR = RGB(34, 197, 94);          // Green
const COLORREF SUCCESS_HOVER = RGB(74, 222, 128);
const COLORREF DANGER_COLOR = RGB(239, 68, 68);           // Red
const COLORREF DANGER_HOVER = RGB(248, 113, 113);
const COLORREF TEXT_PRIMARY = RGB(15, 23, 42);            // Dark text
const COLORREF TEXT_SECONDARY = RGB(100, 116, 139);       // Muted text
const COLORREF BORDER_DEFAULT = RGB(226, 232, 240);       // Light border
const COLORREF SHADOW_COLOR = RGB(148, 163, 184);         // Shadow
const COLORREF BG_DIALOG = RGB(255, 255, 255);            // Dialog white

// ============================================================================
// CONTROL IDs
// ============================================================================

enum ControlID {
    BTN_OPEN = 101,
    BTN_SAVE = 102,
    BTN_RECORD = 103,
    BTN_PLAY = 104,
    BTN_STOP_ALL = 105,
    BTN_SETTINGS = 106,
    BTN_TOGGLE_CLICKER = 107,
    STATIC_STATUS = 108,
    
    MENU_SPEED_HALF = 201,
    MENU_SPEED_1X = 202,
    MENU_SPEED_2X = 203,
    MENU_SPEED_100X = 204,
    MENU_SPEED_CUSTOM = 205,
    MENU_CONTINUOUS = 207,
    MENU_SET_LOOPS = 208,
    MENU_CLICK_INTERVAL = 209,
    MENU_HUMANIZATION = 210,
    MENU_ALWAYS_ON_TOP = 211,
    MENU_CLEAR_RECORDING = 212,
    MENU_ABOUT = 213,
    MENU_CUSTOMIZE_HOTKEYS = 214,
    
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
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

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
    UINT hotkeyRecord = 'F';
    UINT hotkeyPlayback = 'P';
    UINT hotkeyClicker = VK_F6;
    UINT hotkeyStop = VK_PAUSE;
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;  // For playback
} g_app;

// Temporary hotkey storage for customization dialog
UINT g_tempHotkeyRecord = 'F';
UINT g_tempHotkeyPlayback = 'P';
UINT g_tempHotkeyClicker = VK_F6;
UINT g_tempHotkeyStop = VK_PAUSE;

// Hotkey edit control handles
HWND g_hHotkeyRecordEdit = nullptr;
HWND g_hHotkeyPlaybackEdit = nullptr;
HWND g_hHotkeyClickerEdit = nullptr;
HWND g_hHotkeyStopEdit = nullptr;

// ============================================================================
// MODERN ROUNDED RECTANGLE HELPER
// ============================================================================

void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF fillColor, COLORREF borderColor = 0, int borderWidth = 0) {
    // Create rounded region
    HRGN hRgn = CreateRoundRectRgn(rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    
    // Fill
    HBRUSH brush = CreateSolidBrush(fillColor);
    FillRgn(hdc, hRgn, brush);
    DeleteObject(brush);
    
    // Optional border
    if (borderWidth > 0 && borderColor != 0) {
        HPEN pen = CreatePen(PS_SOLID, borderWidth, borderColor);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
    }
    
    DeleteObject(hRgn);
}

// ============================================================================
// MODERN ICON DRAWING FUNCTIONS
// ============================================================================

void DrawModernFolderIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int s = size / 3;
    
    // Folder body
    RECT body = {cx - s, cy - s/2, cx + s, cy + s};
    DrawRoundedRect(hdc, body, 4, color);
    
    // Folder tab
    RECT tab = {cx - s, cy - s/2 - 6, cx, cy - s/2};
    DrawRoundedRect(hdc, tab, 3, color);
}

void DrawModernSaveIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int s = size / 3;
    
    // Disk body
    RECT body = {cx - s, cy - s, cx + s, cy + s};
    DrawRoundedRect(hdc, body, 4, color);
    
    // Disk notch
    RECT notch = {cx - s + 4, cy - s, cx + s - 4, cy - s/2};
    DrawRoundedRect(hdc, notch, 2, BG_ELEVATED);
}

void DrawModernRecordIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 5;
    
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

void DrawModernPlayIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int s = size / 4;
    
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    
    POINT tri[3] = {
        {cx - s/2, cy - s},
        {cx - s/2, cy + s},
        {cx + s, cy}
    };
    Polygon(hdc, tri, 3);
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

void DrawModernStopIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int s = size / 5;
    
    RECT square = {cx - s, cy - s, cx + s, cy + s};
    DrawRoundedRect(hdc, square, 3, color);
}

void DrawModernSettingsIcon(HDC hdc, int x, int y, int size, COLORREF color) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 6;
    
    // Center circle
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    
    Ellipse(hdc, cx - r/2, cy - r/2, cx + r/2, cy + r/2);
    
    // Gear teeth (6 rectangles around center)
    for (int i = 0; i < 6; i++) {
        double angle = i * 3.14159265 / 3.0;
        int x1 = cx + (int)(r * 1.8 * cos(angle));
        int y1 = cy + (int)(r * 1.8 * sin(angle));
        RECT tooth = {x1 - 3, y1 - 6, x1 + 3, y1 + 6};
        
        // Rotate rectangle
        XFORM xform;
        SetGraphicsMode(hdc, GM_ADVANCED);
        GetWorldTransform(hdc, &xform);
        
        XFORM rot;
        rot.eM11 = (FLOAT)cos(angle);
        rot.eM12 = (FLOAT)sin(angle);
        rot.eM21 = (FLOAT)-sin(angle);
        rot.eM22 = (FLOAT)cos(angle);
        rot.eDx = (FLOAT)cx;
        rot.eDy = (FLOAT)cy;
        SetWorldTransform(hdc, &rot);
        
        Rectangle(hdc, x1 - cx - 3, y1 - cy - 6, x1 - cx + 3, y1 - cy + 6);
        
        SetWorldTransform(hdc, &xform);
        SetGraphicsMode(hdc, GM_COMPATIBLE);
    }
    
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

// ============================================================================
// MODERN BUTTON CREATION
// ============================================================================

HWND CreateModernButton(HWND parent, int x, int y, int id, 
                        void (*drawFunc)(HDC, int, int, int, COLORREF),
                        COLORREF color, const wchar_t* tooltip) {
    HWND btn = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, BUTTON_SIZE, BUTTON_SIZE,
        parent, (HMENU)(LONG_PTR)id, GetModuleHandle(NULL), NULL);
    
    // Store draw function pointer
    SetWindowLongPtrW(btn, GWLP_USERDATA, (LONG_PTR)drawFunc);
    
    // Create tooltip
    if (tooltip) {
        HWND hwndTT = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            parent, NULL, GetModuleHandle(NULL), NULL);
        
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(TOOLINFOW);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = parent;
        ti.uId = (UINT_PTR)btn;
        ti.lpszText = (LPWSTR)tooltip;
        SendMessageW(hwndTT, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }
    
    return btn;
}

// ============================================================================
// STATUS DISPLAY
// ============================================================================

void UpdateStatusDisplay() {
    HWND hStatus = GetDlgItem(g_app.hwnd, STATIC_STATUS);
    if (!hStatus) return;
    
    std::wstringstream ss;
    
    if (g_app.isRecording) {
        ss << L"● RECORDING";
    } else if (g_app.isPlaying) {
        ss << L"▶ PLAYING";
    } else if (g_app.isClicking) {
        ss << L"⚡ AUTO-CLICKING (" << g_app.clickInterval << L"ms)";
    } else {
        ss << L"● Ready";
    }
    
    ss << L"  |  Speed: " << g_app.playbackSpeed << L"x";
    if (g_app.loopCount > 1 && !g_app.continuous) {
        ss << L"  |  Loops: " << g_app.loopCount;
    } else if (g_app.continuous) {
        ss << L"  |  Loops: ∞";
    }
    
    SetWindowTextW(hStatus, ss.str().c_str());
}

// ============================================================================
// SETTINGS MENU
// ============================================================================

void ShowSettingsMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    // Playback Speed
    HMENU hSpeedMenu = CreatePopupMenu();
    AppendMenuW(hSpeedMenu, MF_STRING | (g_app.playbackSpeed == 0.5f ? MF_CHECKED : 0), 
               MENU_SPEED_HALF, L"Play Speed: 1/2x");
    AppendMenuW(hSpeedMenu, MF_STRING | (g_app.playbackSpeed == 1.0f ? MF_CHECKED : 0), 
               MENU_SPEED_1X, L"Play Speed: 1x");
    AppendMenuW(hSpeedMenu, MF_STRING | (g_app.playbackSpeed == 2.0f ? MF_CHECKED : 0), 
               MENU_SPEED_2X, L"Play Speed: 2x");
    AppendMenuW(hSpeedMenu, MF_STRING | (g_app.playbackSpeed == 100.0f ? MF_CHECKED : 0), 
               MENU_SPEED_100X, L"Play Speed: 100x");
    AppendMenuW(hSpeedMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hSpeedMenu, MF_STRING, MENU_SPEED_CUSTOM, L"Set Custom Speed...");
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSpeedMenu, L"Playback Speed");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Playback Options
    AppendMenuW(hMenu, MF_STRING | (g_app.continuous ? MF_CHECKED : 0), 
               MENU_CONTINUOUS, L"🔁 Continuous Playback");
    
    // Show current loop count in menu
    wchar_t loopMenuText[64];
    swprintf(loopMenuText, 64, L"🔢 Set Playback Loops...  (%d)", g_app.loopCount);
    AppendMenuW(hMenu, MF_STRING, MENU_SET_LOOPS, loopMenuText);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Auto-Clicker
    wchar_t intervalMenuText[64];
    swprintf(intervalMenuText, 64, L"⚡ Auto-Clicker Interval...  (%d ms)", g_app.clickInterval);
    AppendMenuW(hMenu, MF_STRING, MENU_CLICK_INTERVAL, intervalMenuText);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Humanization
    HMENU hHumanMenu = CreatePopupMenu();
    AppendMenuW(hHumanMenu, MF_STRING | (g_app.humanizationEnabled ? MF_CHECKED : 0),
               MENU_HUMANIZATION, L"✨ Enable Humanization");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHumanMenu, L"🎭 Humanization");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Recording
    AppendMenuW(hMenu, MF_STRING, MENU_CLEAR_RECORDING, L"🗑 Clear Recording");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Hotkeys
    AppendMenuW(hMenu, MF_STRING, MENU_CUSTOMIZE_HOTKEYS, L"⌨ Customize Hotkeys...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Window Options
    AppendMenuW(hMenu, MF_STRING | (g_app.alwaysOnTop ? MF_CHECKED : 0), 
               MENU_ALWAYS_ON_TOP, L"📌 Always on Top");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_ABOUT, L"ℹ About FLOW");
    
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void UpdateWindowState() {
    SetWindowPos(g_app.hwnd, g_app.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

void OpenMacroFile(HWND hwnd) {
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"FLOW Macro Files (*.rec)\0*.rec\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Open Macro File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        if (!g_app.engine->LoadMacro(szFile)) {
            MessageBoxA(hwnd, "Failed to load macro file!", "Error", MB_OK | MB_ICONERROR);
        }
    }
}

void SaveMacroFile(HWND hwnd) {
    wchar_t szFile[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"FLOW Macro Files (*.rec)\0*.rec\0All Files (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Save Macro File";
    ofn.lpstrDefExt = L"rec";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        if (!g_app.engine->SaveMacro(szFile)) {
            MessageBoxA(hwnd, "Failed to save macro file!", "Error", MB_OK | MB_ICONERROR);
        }
    }
}

// ============================================================================
// ACTIONS
// ============================================================================

void ToggleRecording(HWND hwnd) {
    if (g_app.isRecording) {
        g_app.engine->StopRecording();
        g_app.isRecording = false;
        SetWindowTextA(hwnd, "FLOW");
    } else {
        g_app.engine->StartRecording();
        g_app.isRecording = true;
        SetWindowTextA(hwnd, "FLOW - Recording...");
    }
    UpdateStatusDisplay();
    InvalidateRect(hwnd, NULL, TRUE);
}

void TogglePlayback(HWND hwnd) {
    if (g_app.isPlaying) {
        g_app.engine->StopPlayback();
        g_app.isPlaying = false;
        SetWindowTextA(hwnd, "FLOW");
    } else {
        if (g_app.continuous) {
            g_app.engine->StartPlayback(999999);
        } else {
            g_app.engine->StartPlayback(g_app.loopCount);
        }
        g_app.isPlaying = true;
        SetWindowTextA(hwnd, "FLOW - Playing...");
    }
    UpdateStatusDisplay();
    InvalidateRect(hwnd, NULL, TRUE);
}

void ToggleAutoClicker() {
    if (g_app.isClicking) {
        g_app.engine->StopAutoClicker();
        g_app.isClicking = false;
        // Force update after stop
        Sleep(50);
    } else {
        g_app.engine->StartAutoClicker(g_app.clickInterval);
        g_app.isClicking = true;
    }
    UpdateStatusDisplay();
    InvalidateRect(g_app.hwnd, NULL, TRUE);
}

void StopAll(HWND hwnd) {
    // Stop recording if active
    if (g_app.isRecording) {
        g_app.engine->StopRecording();
        g_app.isRecording = false;
    }
    
    // Stop playback if active
    if (g_app.isPlaying) {
        g_app.engine->StopPlayback();
        g_app.isPlaying = false;
    }
    
    // Stop auto-clicker if active
    if (g_app.isClicking) {
        g_app.engine->StopAutoClicker();
        g_app.isClicking = false;
        Sleep(50);
    }
    
    SetWindowTextA(hwnd, "FLOW");
    UpdateStatusDisplay();
    InvalidateRect(hwnd, NULL, TRUE);
}

void RegisterHotkeys() {
    RegisterHotKey(g_app.hwnd, HOTKEY_RECORD, 0, g_app.hotkeyRecord);
    RegisterHotKey(g_app.hwnd, HOTKEY_PLAYBACK, g_app.hotkeyModifiers, g_app.hotkeyPlayback);
    RegisterHotKey(g_app.hwnd, HOTKEY_CLICKER, 0, g_app.hotkeyClicker);
    RegisterHotKey(g_app.hwnd, HOTKEY_STOP, 0, VK_PAUSE);
}

void UnregisterHotkeys() {
    UnregisterHotKey(g_app.hwnd, HOTKEY_RECORD);
    UnregisterHotKey(g_app.hwnd, HOTKEY_PLAYBACK);
    UnregisterHotKey(g_app.hwnd, HOTKEY_CLICKER);
    UnregisterHotKey(g_app.hwnd, HOTKEY_STOP);
}

const char* GetKeyName(UINT vk, bool withModifiers = false) {
    static char keyName[64];
    keyName[0] = '\0';
    
    if (withModifiers && g_app.hotkeyModifiers) {
        if (g_app.hotkeyModifiers & MOD_CONTROL) strcat(keyName, "Ctrl+");
        if (g_app.hotkeyModifiers & MOD_SHIFT) strcat(keyName, "Shift+");
        if (g_app.hotkeyModifiers & MOD_ALT) strcat(keyName, "Alt+");
    }
    
    char key[32];
    switch (vk) {
        case VK_F1: strcpy(key, "F1"); break;
        case VK_F2: strcpy(key, "F2"); break;
        case VK_F3: strcpy(key, "F3"); break;
        case VK_F4: strcpy(key, "F4"); break;
        case VK_F5: strcpy(key, "F5"); break;
        case VK_F6: strcpy(key, "F6"); break;
        case VK_F7: strcpy(key, "F7"); break;
        case VK_F8: strcpy(key, "F8"); break;
        case VK_F9: strcpy(key, "F9"); break;
        case VK_F10: strcpy(key, "F10"); break;
        case VK_F11: strcpy(key, "F11"); break;
        case VK_F12: strcpy(key, "F12"); break;
        case VK_PAUSE: strcpy(key, "Pause"); break;
        default:
            if (vk >= 'A' && vk <= 'Z') {
                sprintf(key, "%c", vk);
            } else if (vk >= '0' && vk <= '9') {
                sprintf(key, "%c", vk);
            } else {
                sprintf(key, "Key %d", vk);
            }
            break;
    }
    strcat(keyName, key);
    return keyName;
}

// ============================================================================
// DIALOG BOXES
// ============================================================================

// Dialog controls
#define IDC_LOOP_EDIT 1001
#define IDC_LOOP_OK 1002
#define IDC_LOOP_CANCEL 1003
#define IDC_INTERVAL_EDIT 1004
#define IDC_INTERVAL_OK 1005
#define IDC_INTERVAL_CANCEL 1006
#define IDC_HOTKEY_RECORD 1007
#define IDC_HOTKEY_PLAYBACK 1008
#define IDC_HOTKEY_CLICKER 1009
#define IDC_HOTKEY_OK 1010
#define IDC_HOTKEY_CANCEL 1011

INT_PTR CALLBACK LoopCountDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Center the dialog
            RECT rcDlg, rcOwner;
            GetWindowRect(hDlg, &rcDlg);
            GetWindowRect(GetParent(hDlg), &rcOwner);
            int x = rcOwner.left + (rcOwner.right - rcOwner.left - (rcDlg.right - rcDlg.left)) / 2;
            int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - (rcDlg.bottom - rcDlg.top)) / 2;
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            // Set current loop count in edit box
            char buffer[32];
            sprintf(buffer, "%d", g_app.loopCount);
            SetDlgItemTextA(hDlg, IDC_LOOP_EDIT, buffer);
            
            // Select all text
            SendDlgItemMessageA(hDlg, IDC_LOOP_EDIT, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hDlg, IDC_LOOP_EDIT));
            return FALSE; // We set the focus
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_LOOP_OK:
                case IDOK: {
                    char buffer[32];
                    GetDlgItemTextA(hDlg, IDC_LOOP_EDIT, buffer, sizeof(buffer));
                    int value = atoi(buffer);
                    if (value >= 1 && value <= 999) {
                        g_app.loopCount = value;
                        UpdateStatusDisplay();
                        EndDialog(hDlg, IDOK);
                    } else {
                        MessageBoxA(hDlg, "Please enter a value between 1 and 999.", "Invalid Input", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hDlg, IDC_LOOP_EDIT));
                    }
                    return TRUE;
                }
                case IDC_LOOP_CANCEL:
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void ShowCustomSpeedDialog(HWND hwnd) {
    char buffer[64];
    sprintf(buffer, "%.2f", g_app.playbackSpeed);
    
    // For simplicity, use MessageBox instead
    if (MessageBoxA(hwnd, "Enter custom playback speed\n(Use Settings menu to configure)",
                    "Custom Speed", MB_OK | MB_ICONINFORMATION) == IDOK) {
        // Custom dialog would go here
    }
}

// Global variables for dialog
static HWND g_hLoopEdit = NULL;

LRESULT CALLBACK LoopDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_LOOP_OK: {
                    char buffer[32];
                    GetWindowTextA(g_hLoopEdit, buffer, sizeof(buffer));
                    int value = atoi(buffer);
                    if (value >= 1 && value <= 999) {
                        g_app.loopCount = value;
                        UpdateStatusDisplay();
                        EndDialog(hDlg, IDOK);
                    } else {
                        MessageBoxA(hDlg, "Please enter a value between 1 and 999.", "Invalid Input", MB_OK | MB_ICONWARNING);
                    }
                    return TRUE;
                }
                case IDC_LOOP_CANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void ShowLoopCountDialog(HWND hwnd) {
    // Use DialogBox with inline template
    struct {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD windowClass;
        WCHAR title[32];
        WORD fontSize;
        WCHAR fontName[16];
    } template_data;
    
    memset(&template_data, 0, sizeof(template_data));
    template_data.dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER;
    template_data.dlg.dwExtendedStyle = 0;
    template_data.dlg.cdit = 0;
    template_data.dlg.x = 0;
    template_data.dlg.y = 0;
    template_data.dlg.cx = 180;
    template_data.dlg.cy = 70;
    template_data.menu = 0;
    template_data.windowClass = 0;
    wcscpy(template_data.title, L"Set Playback Loops");
    template_data.fontSize = 9;
    wcscpy(template_data.fontName, L"Segoe UI");
    
    // Create the dialog window manually
    HWND hDlg = CreateDialogIndirectParamW(
        GetModuleHandle(NULL),
        &template_data.dlg,
        hwnd,
        (DLGPROC)LoopDialogWndProc,
        0);
    
    if (!hDlg) {
        wchar_t buffer[64];
        swprintf(buffer, 64, L"Current: %d loops\n\nEnter new loop count (1-999):", g_app.loopCount);
        MessageBoxW(hwnd, buffer, L"Set Loop Count", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Professional modern window - larger, clean design
    SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 600, 340, SWP_NOMOVE);
    
    // Modern elevated background
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_ELEVATED));
    
    // Professional typography system
    HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hEditFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    // Modern title with icon
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"🔁 Playback Loop Settings",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 30, 530, 40, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    
    // Clean description
    HWND hLabel = CreateWindowExW(0, L"STATIC", L"Number of playback repetitions (1-999 loops):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 90, 530, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Modern input field with rounded appearance
    wchar_t currentValue[32];
    swprintf(currentValue, 32, L"%d", g_app.loopCount);
    g_hLoopEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", currentValue,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL | ES_CENTER,
        35, 130, 530, 52, hDlg, (HMENU)IDC_LOOP_EDIT, GetModuleHandle(NULL), NULL);
    SendMessage(g_hLoopEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SendMessage(g_hLoopEdit, EM_SETSEL, 0, -1);
    
    // Modern action buttons with proper spacing
    HWND hOK = CreateWindowExW(0, L"BUTTON", L"✓ Apply Changes",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        190, 240, 200, 56, hDlg, (HMENU)IDC_LOOP_OK, GetModuleHandle(NULL), NULL);
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hCancel = CreateWindowExW(0, L"BUTTON", L"✕ Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        400, 240, 165, 56, hDlg, (HMENU)IDC_LOOP_CANCEL, GetModuleHandle(NULL), NULL);
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    DeleteObject(hEditFont);
    
    // Center on parent
    RECT rcDlg, rcParent;
    GetWindowRect(hDlg, &rcDlg);
    GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    
    // Show and activate
    ShowWindow(hDlg, SW_SHOW);
    SetFocus(g_hLoopEdit);
    
    // Modal loop
    EnableWindow(hwnd, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

// Global variable for click interval dialog
static HWND g_hIntervalEdit = NULL;

LRESULT CALLBACK IntervalDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_INTERVAL_OK: {
                    char buffer[32];
                    GetWindowTextA(g_hIntervalEdit, buffer, sizeof(buffer));
                    int value = atoi(buffer);
                    if (value >= 1 && value <= 10000) {
                        g_app.clickInterval = value;
                        UpdateStatusDisplay();
                        EndDialog(hDlg, IDOK);
                    } else {
                        MessageBoxA(hDlg, "Please enter a value between 1 and 10000 ms.", "Invalid Input", MB_OK | MB_ICONWARNING);
                    }
                    return TRUE;
                }
                case IDC_INTERVAL_CANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void ShowClickIntervalDialog(HWND hwnd) {
    // Use DialogBox with inline template
    struct {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD windowClass;
        WCHAR title[32];
        WORD fontSize;
        WCHAR fontName[16];
    } template_data;
    
    memset(&template_data, 0, sizeof(template_data));
    template_data.dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER;
    template_data.dlg.dwExtendedStyle = 0;
    template_data.dlg.cdit = 0;
    template_data.dlg.x = 0;
    template_data.dlg.y = 0;
    template_data.dlg.cx = 180;
    template_data.dlg.cy = 70;
    template_data.menu = 0;
    template_data.windowClass = 0;
    wcscpy(template_data.title, L"Auto-Clicker Interval");
    template_data.fontSize = 9;
    wcscpy(template_data.fontName, L"Segoe UI");
    
    // Create the dialog window manually
    HWND hDlg = CreateDialogIndirectParamW(
        GetModuleHandle(NULL),
        &template_data.dlg,
        hwnd,
        (DLGPROC)IntervalDialogWndProc,
        0);
    
    if (!hDlg) {
        wchar_t buffer[64];
        swprintf(buffer, 64, L"Current: %d ms\n\nEnter new interval (1-10000):", g_app.clickInterval);
        MessageBoxW(hwnd, buffer, L"Auto-Clicker Interval", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Professional modern dialog - spacious layout
    SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 620, 350, SWP_NOMOVE);
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_ELEVATED));
    
    // Professional typography
    HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hEditFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    // Modern title
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"⚡ Auto-Clicker Settings",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 30, 550, 40, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    
    // Clean description
    HWND hLabel = CreateWindowExW(0, L"STATIC", L"Click interval in milliseconds (1-10000 ms):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 90, 550, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Professional input field
    wchar_t currentValue[32];
    swprintf(currentValue, 32, L"%d", g_app.clickInterval);
    g_hIntervalEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", currentValue,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL | ES_CENTER,
        35, 130, 550, 52, hDlg, (HMENU)IDC_INTERVAL_EDIT, GetModuleHandle(NULL), NULL);
    SendMessage(g_hIntervalEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SendMessage(g_hIntervalEdit, EM_SETSEL, 0, -1);
    
    // Helpful hint
    HWND hHint = CreateWindowExW(0, L"STATIC", L"💡 Tip: 100ms = 10 clicks/second, 1000ms = 1 click/second",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 195, 550, 24, hDlg, NULL, GetModuleHandle(NULL), NULL);
    HFONT hSmallFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(hHint, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
    
    // Modern action buttons
    HWND hOK = CreateWindowExW(0, L"BUTTON", L"✓ Apply",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        210, 250, 200, 56, hDlg, (HMENU)IDC_INTERVAL_OK, GetModuleHandle(NULL), NULL);
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hCancel = CreateWindowExW(0, L"BUTTON", L"✕ Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        420, 250, 165, 56, hDlg, (HMENU)IDC_INTERVAL_CANCEL, GetModuleHandle(NULL), NULL);
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    DeleteObject(hSmallFont);
    DeleteObject(hEditFont);
    
    // Center on parent
    RECT rcDlg, rcParent;
    GetWindowRect(hDlg, &rcDlg);
    GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    
    // Show and activate
    ShowWindow(hDlg, SW_SHOW);
    SetFocus(g_hIntervalEdit);
    
    // Modal loop
    EnableWindow(hwnd, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

// Hotkey dialog callback
LRESULT CALLBACK HotkeyDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_HOTKEY_OK: {
                    // Validate hotkeys
                    if (g_tempHotkeyRecord == 0 || g_tempHotkeyPlayback == 0 || g_tempHotkeyClicker == 0) {
                        MessageBoxA(hDlg, "Please set all hotkeys!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // Check for duplicates
                    if (g_tempHotkeyRecord == g_tempHotkeyPlayback || 
                        g_tempHotkeyRecord == g_tempHotkeyClicker ||
                        g_tempHotkeyPlayback == g_tempHotkeyClicker) {
                        MessageBoxA(hDlg, "Hotkeys must be unique!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // Unregister old hotkeys
                    UnregisterHotkeys();
                    
                    // Apply new hotkeys
                    g_app.hotkeyRecord = g_tempHotkeyRecord;
                    g_app.hotkeyPlayback = g_tempHotkeyPlayback;
                    g_app.hotkeyClicker = g_tempHotkeyClicker;
                    
                    // Register new hotkeys
                    RegisterHotkeys();
                    
                    DestroyWindow(hDlg);
                    break;
                }
                case IDC_HOTKEY_CANCEL:
                    DestroyWindow(hDlg);
                    break;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
            
        case WM_DESTROY:
            g_hHotkeyRecordEdit = NULL;
            g_hHotkeyPlaybackEdit = NULL;
            g_hHotkeyClickerEdit = NULL;
            PostQuitMessage(0);
            break;
    }
    return 0;
}

LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_KEYDOWN) {
        UINT vk = (UINT)wParam;
        
        // Allow F1-F12, A-Z, 0-9
        bool validKey = (vk >= VK_F1 && vk <= VK_F12) || 
                        (vk >= 'A' && vk <= 'Z') || 
                        (vk >= '0' && vk <= '9');
        
        if (validKey) {
            // Store the hotkey based on which edit control this is
            if (hwnd == g_hHotkeyRecordEdit) {
                g_tempHotkeyRecord = vk;
            } else if (hwnd == g_hHotkeyPlaybackEdit) {
                g_tempHotkeyPlayback = vk;
            } else if (hwnd == g_hHotkeyClickerEdit) {
                g_tempHotkeyClicker = vk;
            }
            
            // Display the key name (with modifiers for playback)
            bool showModifiers = (hwnd == g_hHotkeyPlaybackEdit);
            SetWindowTextA(hwnd, GetKeyName(vk, showModifiers));
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ShowCustomizeHotkeysDialog(HWND hwnd) {
    // Initialize temp hotkeys with current values
    g_tempHotkeyRecord = g_app.hotkeyRecord;
    g_tempHotkeyPlayback = g_app.hotkeyPlayback;
    g_tempHotkeyClicker = g_app.hotkeyClicker;
    g_tempHotkeyStop = g_app.hotkeyStop;
    
    struct {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD windowClass;
        WCHAR title[32];
        WORD fontSize;
        WCHAR fontName[32];
    } template_data;
    
    memset(&template_data, 0, sizeof(template_data));
    template_data.dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER;
    template_data.dlg.dwExtendedStyle = 0;
    template_data.dlg.cdit = 0;
    template_data.dlg.x = 0;
    template_data.dlg.y = 0;
    template_data.dlg.cx = 260;
    template_data.dlg.cy = 170;
    template_data.menu = 0;
    template_data.windowClass = 0;
    wcscpy(template_data.title, L"Customize Hotkeys");
    template_data.fontSize = 14;
    wcscpy(template_data.fontName, L"Segoe UI");
    
    // Create the dialog window
    HWND hDlg = CreateDialogIndirectParamW(
        GetModuleHandle(NULL),
        &template_data.dlg,
        hwnd,
        (DLGPROC)HotkeyDialogWndProc,
        0);
    
    if (!hDlg) {
        MessageBoxW(hwnd, L"Failed to create hotkey dialog", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Professional modern dialog - card-style layout
    SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 680, 560, SWP_NOMOVE);
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_ELEVATED));
    
    // Professional typography system
    HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hEditFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    // Modern title
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"⌨ Keyboard Shortcuts",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 30, 610, 40, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    
    // Recording hotkey - modern card style
    HWND hLabel1 = CreateWindowExW(0, L"STATIC", L"● Recording:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 95, 180, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel1, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    const char* keyName1 = GetKeyName(g_app.hotkeyRecord, false);
    wchar_t wKeyName1[64];
    MultiByteToWideChar(CP_ACP, 0, keyName1, -1, wKeyName1, 64);
    g_hHotkeyRecordEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wKeyName1,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
        220, 90, 425, 48, hDlg, (HMENU)IDC_HOTKEY_RECORD, GetModuleHandle(NULL), NULL);
    SendMessage(g_hHotkeyRecordEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SetWindowSubclass(g_hHotkeyRecordEdit, HotkeyEditProc, 0, 0);
    
    // Playback hotkey
    HWND hLabel2 = CreateWindowExW(0, L"STATIC", L"▶ Playback:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 165, 180, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel2, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    const char* keyName2 = GetKeyName(g_app.hotkeyPlayback, true);
    wchar_t wKeyName2[64];
    MultiByteToWideChar(CP_ACP, 0, keyName2, -1, wKeyName2, 64);
    g_hHotkeyPlaybackEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wKeyName2,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
        220, 160, 425, 48, hDlg, (HMENU)IDC_HOTKEY_PLAYBACK, GetModuleHandle(NULL), NULL);
    SendMessage(g_hHotkeyPlaybackEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SetWindowSubclass(g_hHotkeyPlaybackEdit, HotkeyEditProc, 0, 0);
    
    // Auto-Clicker hotkey
    HWND hLabel3 = CreateWindowExW(0, L"STATIC", L"⚡ Auto-Clicker:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 235, 180, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel3, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    const char* keyName3 = GetKeyName(g_app.hotkeyClicker, false);
    wchar_t wKeyName3[64];
    MultiByteToWideChar(CP_ACP, 0, keyName3, -1, wKeyName3, 64);
    g_hHotkeyClickerEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wKeyName3,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
        220, 230, 425, 48, hDlg, (HMENU)IDC_HOTKEY_CLICKER, GetModuleHandle(NULL), NULL);
    SendMessage(g_hHotkeyClickerEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SetWindowSubclass(g_hHotkeyClickerEdit, HotkeyEditProc, 0, 0);
    
    // Stop All hotkey
    HWND hLabel4 = CreateWindowExW(0, L"STATIC", L"■ Stop All:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        35, 305, 180, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hLabel4, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    const char* keyName4 = GetKeyName(g_app.hotkeyStop, false);
    wchar_t wKeyName4[64];
    MultiByteToWideChar(CP_ACP, 0, keyName4, -1, wKeyName4, 64);
    g_hHotkeyStopEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wKeyName4,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_CENTER,
        220, 300, 425, 48, hDlg, (HMENU)IDC_HOTKEY_STOP, GetModuleHandle(NULL), NULL);
    SendMessage(g_hHotkeyStopEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    SetWindowSubclass(g_hHotkeyStopEdit, HotkeyEditProc, 0, 0);
    
    // Helpful instructions at bottom
    HFONT hSmallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HWND hInstr = CreateWindowExW(0, L"STATIC", L"💡 Click any field and press your desired key (F1-F12, A-Z, 0-9)",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        35, 385, 610, 28, hDlg, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(hInstr, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
    
    // Modern action buttons with proper spacing
    HWND hOK = CreateWindowExW(0, L"BUTTON", L"✓ Save",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        230, 450, 200, 58, hDlg, (HMENU)IDC_HOTKEY_OK, GetModuleHandle(NULL), NULL);
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    HWND hCancel = CreateWindowExW(0, L"BUTTON", L"✕ Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        440, 450, 165, 58, hDlg, (HMENU)IDC_HOTKEY_CANCEL, GetModuleHandle(NULL), NULL);
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    DeleteObject(hSmallFont);
    DeleteObject(hEditFont);
    
    // Center on parent
    RECT rcDlg, rcParent;
    GetWindowRect(hDlg, &rcDlg);
    GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    
    // Show and activate
    ShowWindow(hDlg, SW_SHOW);
    SetFocus(g_hHotkeyRecordEdit);
    
    // Modal loop
    EnableWindow(hwnd, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);
    
    DeleteObject(hSmallFont);
    DeleteObject(hFont);
    DeleteObject(hTitleFont);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            switch (id) {
                case BTN_OPEN:
                    OpenMacroFile(hwnd);
                    break;
                case BTN_SAVE:
                    SaveMacroFile(hwnd);
                    break;
                case BTN_RECORD:
                    ToggleRecording(hwnd);
                    break;
                case BTN_PLAY:
                    TogglePlayback(hwnd);
                    break;
                case BTN_STOP_ALL:
                    StopAll(hwnd);
                    break;
                case BTN_SETTINGS:
                    ShowSettingsMenu(hwnd);
                    break;
                case BTN_TOGGLE_CLICKER:
                    ToggleAutoClicker();
                    break;
                    
                case MENU_SPEED_HALF:
                    g_app.playbackSpeed = 0.5f;
                    UpdateStatusDisplay();
                    break;
                case MENU_SPEED_1X:
                    g_app.playbackSpeed = 1.0f;
                    UpdateStatusDisplay();
                    break;
                case MENU_SPEED_2X:
                    g_app.playbackSpeed = 2.0f;
                    UpdateStatusDisplay();
                    break;
                case MENU_SPEED_100X:
                    g_app.playbackSpeed = 100.0f;
                    UpdateStatusDisplay();
                    break;
                case MENU_SPEED_CUSTOM:
                    ShowCustomSpeedDialog(hwnd);
                    break;
                case MENU_CONTINUOUS:
                    g_app.continuous = !g_app.continuous;
                    UpdateStatusDisplay();
                    break;
                case MENU_SET_LOOPS:
                    ShowLoopCountDialog(hwnd);
                    break;
                case MENU_CLICK_INTERVAL:
                    ShowClickIntervalDialog(hwnd);
                    break;
                case MENU_HUMANIZATION:
                    g_app.humanizationEnabled = !g_app.humanizationEnabled;
                    g_app.engine->EnableHumanization(g_app.humanizationEnabled);
                    if (g_app.humanizationEnabled) {
                        g_app.engine->ConfigureHumanization(0.0, g_app.humanizationStdDev);
                    }
                    break;
                case MENU_CLEAR_RECORDING:
                    g_app.engine->ClearRecording();
                    MessageBoxA(hwnd, "Recording cleared!", "Info", MB_OK | MB_ICONINFORMATION);
                    break;
                case MENU_CUSTOMIZE_HOTKEYS:
                    ShowCustomizeHotkeysDialog(hwnd);
                    break;
                case MENU_ALWAYS_ON_TOP:
                    g_app.alwaysOnTop = !g_app.alwaysOnTop;
                    UpdateWindowState();
                    break;
                case MENU_ABOUT:
                    MessageBoxW(hwnd, 
                        L"FLOW v2.0.0\n\n"
                        L"Flexible Low-latency Operations Workflow\n\n"
                        L"A powerful macro automation tool\n\n"
                        L"Default Hotkeys:\n\n"
                        L"\u25cf  F8 - Start/Stop Recording\n"
                        L"\u25b6  Ctrl+Shift+Alt+P - Start/Stop Playback\n"
                        L"\u26a1  F6 - Toggle Auto-Clicker\n"
                        L"\u25a0  Pause - Stop All Activities\n\n"
                        L"Customize hotkeys via Settings menu", 
                        L"About FLOW", MB_OK | MB_ICONINFORMATION);
                    break;
            }
            break;
        }
        
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                HDC hdc = dis->hDC;
                RECT rect = dis->rcItem;
                
                // Anti-aliasing mode
                SetBkMode(hdc, TRANSPARENT);
                
                // Get button state
                bool isPressed = (dis->itemState & ODS_SELECTED);
                bool isHot = (dis->itemState & ODS_HOTLIGHT);
                
                // Determine if button is active
                bool isActive = false;
                COLORREF baseColor = ACCENT_PRIMARY;
                
                if (dis->CtlID == BTN_RECORD) {
                    isActive = g_app.isRecording;
                    baseColor = DANGER_COLOR;
                } else if (dis->CtlID == BTN_PLAY) {
                    isActive = g_app.isPlaying;
                    baseColor = SUCCESS_COLOR;
                } else if (dis->CtlID == BTN_STOP_ALL) {
                    isActive = (g_app.isRecording || g_app.isPlaying || g_app.isClicking);
                    baseColor = DANGER_COLOR;
                }
                
                // Modern button colors
                COLORREF bgColor, iconColor;
                if (isActive) {
                    // Active state - filled with color
                    bgColor = baseColor;
                    iconColor = RGB(255, 255, 255);
                } else if (isPressed) {
                    // Pressed - darker
                    bgColor = RGB(226, 232, 240);
                    iconColor = TEXT_PRIMARY;
                } else if (isHot) {
                    // Hover - light elevation
                    bgColor = RGB(241, 245, 249);
                    iconColor = baseColor;
                } else {
                    // Default - white with subtle border
                    bgColor = BG_ELEVATED;
                    iconColor = TEXT_SECONDARY;
                }
                
                // Draw modern rounded button
                DrawRoundedRect(hdc, rect, CORNER_RADIUS, bgColor, BORDER_DEFAULT, 1);
                
                // Draw subtle shadow when not pressed
                if (!isPressed && isHot) {
                    RECT shadowRect = {rect.left + 2, rect.top + 2, rect.right + 2, rect.bottom + 2};
                    HBRUSH shadowBrush = CreateSolidBrush(RGB(226, 232, 240));
                    HRGN shadowRgn = CreateRoundRectRgn(shadowRect.left, shadowRect.top, 
                                                        shadowRect.right, shadowRect.bottom, 
                                                        CORNER_RADIUS, CORNER_RADIUS);
                    HRGN buttonRgn = CreateRoundRectRgn(rect.left, rect.top, rect.right, rect.bottom,
                                                        CORNER_RADIUS, CORNER_RADIUS);
                    CombineRgn(shadowRgn, shadowRgn, buttonRgn, RGN_DIFF);
                    FillRgn(hdc, shadowRgn, shadowBrush);
                    DeleteObject(shadowBrush);
                    DeleteObject(shadowRgn);
                    DeleteObject(buttonRgn);
                }
                
                // Draw icon
                void (*drawFunc)(HDC, int, int, int, COLORREF) = 
                    (void (*)(HDC, int, int, int, COLORREF))GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA);
                
                if (drawFunc) {
                    drawFunc(hdc, rect.left, rect.top, BUTTON_SIZE, iconColor);
                }
            }
            return TRUE;
        }
        
        case WM_HOTKEY: {
            if (wParam == HOTKEY_RECORD) {
                ToggleRecording(hwnd);
            } else if (wParam == HOTKEY_PLAYBACK) {
                TogglePlayback(hwnd);
            } else if (wParam == HOTKEY_CLICKER) {
                ToggleAutoClicker();
            } else if (wParam == HOTKEY_STOP) {
                StopAll(hwnd);
            }
            break;
        }
        
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
            
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, TEXT_PRIMARY);
            SetBkColor(hdcStatic, BG_PRIMARY);
            return (INT_PTR)CreateSolidBrush(BG_PRIMARY);
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// CREATE CONTROLS
// ============================================================================

void CreateControls(HWND hwnd) {
    int x = MARGIN;
    int y = MARGIN;
    
    // Create modern toolbar buttons
    CreateModernButton(hwnd, x, y, BTN_OPEN, DrawModernFolderIcon, ACCENT_PRIMARY, L"📁 Open Recording");
    x += BUTTON_SIZE + BUTTON_SPACING;
    
    CreateModernButton(hwnd, x, y, BTN_SAVE, DrawModernSaveIcon, SUCCESS_COLOR, L"💾 Save Recording");
    x += BUTTON_SIZE + BUTTON_SPACING;
    
    CreateModernButton(hwnd, x, y, BTN_RECORD, DrawModernRecordIcon, DANGER_COLOR, L"● Record (F8)");
    x += BUTTON_SIZE + BUTTON_SPACING;
    
    CreateModernButton(hwnd, x, y, BTN_PLAY, DrawModernPlayIcon, SUCCESS_COLOR, L"▶ Play (Ctrl+Shift+Alt+P)");
    x += BUTTON_SIZE + BUTTON_SPACING;
    
    CreateModernButton(hwnd, x, y, BTN_STOP_ALL, DrawModernStopIcon, DANGER_COLOR, L"■ Stop All (Pause)");
    x += BUTTON_SIZE + BUTTON_SPACING;
    
    CreateModernButton(hwnd, x, y, BTN_SETTINGS, DrawModernSettingsIcon, TEXT_SECONDARY, L"⚙ Settings");
    
    // Modern status display area
    y = TOOLBAR_HEIGHT + MARGIN;
    
    // Auto-clicker toggle button with modern styling
    HWND hClickerBtn = CreateWindowExW(0, L"BUTTON", L"⚡ Auto-Clicker (F6)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        MARGIN, y, 200, 36,
        hwnd, (HMENU)BTN_TOGGLE_CLICKER, GetModuleHandle(NULL), NULL);
    HFONT hClickerFont = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(hClickerBtn, WM_SETFONT, (WPARAM)hClickerFont, TRUE);
    
    // Modern status text with larger font
    HWND hStatus = CreateWindowExW(0, L"STATIC", L"● Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        MARGIN + 215, y, 400, 36,
        hwnd, (HMENU)STATIC_STATUS, GetModuleHandle(NULL), NULL);
    
    // Set modern font for status display  
    HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Enable high-DPI awareness for crisp rendering
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        auto setDpiFunc = (SetProcessDpiAwarenessContextFunc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpiFunc) {
            setDpiFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
        FreeLibrary(user32);
    }
    
    flow::protection::EnforceProtection();
    
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG_PRIMARY);
    wc.lpszClassName = L"FLOW_Modern";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window registration failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    int windowWidth = (BUTTON_SIZE + BUTTON_SPACING) * 6 + MARGIN * 2;
    int windowHeight = WINDOW_FULL_HEIGHT + 20;
    
    g_app.hwnd = CreateWindowExW(0, L"FLOW_Modern", L"FLOW - Professional Automation Tool",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2,
        windowWidth, windowHeight, NULL, NULL, hInstance, NULL);
    
    if (!g_app.hwnd) {
        MessageBoxW(NULL, L"Window creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    g_app.engine = new FlowEngine();
    
    if (!g_app.engine->InstallHooks()) {
        MessageBoxW(NULL, L"Failed to install input hooks!\n\nRun as Administrator.",
            L"Error", MB_OK | MB_ICONERROR);
        delete g_app.engine;
        return 1;
    }
    
    RegisterHotkeys();
    
    CreateControls(g_app.hwnd);
    ShowWindow(g_app.hwnd, nCmdShow);
    UpdateWindow(g_app.hwnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    UnregisterHotkeys();
    
    g_app.engine->UninstallHooks();
    delete g_app.engine;
    
    return (int)msg.wParam;
}
