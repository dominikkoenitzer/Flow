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
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <cwchar>

// Icon resource ID
#define IDI_APPICON 101

using namespace flow;

// ============================================================================
// DESIGN SYSTEM  (light, refined)
// ============================================================================

// Refined light palette
const COLORREF BG_PRIMARY      = RGB(243, 245, 249);   // window background
const COLORREF BG_ELEVATED     = RGB(255, 255, 255);   // card / dialog surface
const COLORREF ACCENT_PRIMARY  = RGB(37,  99,  235);   // blue 600
const COLORREF ACCENT_HOVER    = RGB(59,  130, 246);   // blue 500
const COLORREF ACCENT_PRESSED  = RGB(29,  78,  216);   // blue 700
const COLORREF SUCCESS_COLOR   = RGB(22,  163, 74);    // green 600
const COLORREF SUCCESS_HOVER   = RGB(34,  197, 94);    // green 500
const COLORREF DANGER_COLOR    = RGB(220, 38,  38);    // red 600
const COLORREF DANGER_HOVER    = RGB(239, 68,  68);    // red 500
const COLORREF TEXT_PRIMARY    = RGB(17,  24,  39);    // near-black
const COLORREF TEXT_SECONDARY  = RGB(100, 116, 139);   // slate 500
const COLORREF TEXT_FAINT      = RGB(148, 163, 184);   // slate 400
const COLORREF BORDER_DEFAULT  = RGB(226, 232, 240);   // slate 200
const COLORREF TRACK_BG        = RGB(241, 245, 249);   // slate 100 (ghost / input)
const COLORREF TRACK_HOVER     = RGB(226, 232, 240);   // slate 200
const COLORREF SHADOW_COLOR    = RGB(148, 163, 184);
const COLORREF BG_DIALOG       = RGB(255, 255, 255);

// Layout (client-area coordinates, fixed-size window)
constexpr int PAD          = 18;   // outer padding
constexpr int GAP          = 14;   // gap between cards
constexpr int COL_W        = 275;  // card column width
constexpr int CARD_RADIUS  = 12;
constexpr int BTN_RADIUS   = 9;
constexpr int HEADER_H     = 58;
constexpr int CLIENT_W     = 600;
constexpr int CLIENT_H     = 426;

// Card geometry
constexpr int CARDS_TOP    = HEADER_H + 6;             // 64
constexpr int LEFT_X       = PAD;                      // 18
constexpr int RIGHT_X      = PAD + COL_W + GAP;        // 307
constexpr int REC_H        = 120;
constexpr int CLK_H        = 150;
constexpr int REC_TOP      = CARDS_TOP;                // 64
constexpr int CLK_TOP      = CARDS_TOP + REC_H + GAP;  // 198
constexpr int PLAY_H       = REC_H + GAP + CLK_H;      // 284
constexpr int FOOTER_TOP   = CARDS_TOP + PLAY_H + GAP; // 362
constexpr int FOOTER_H     = 46;

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

    EDIT_SPEED = 120,
    EDIT_LOOPS = 121,
    EDIT_INTERVAL = 122,
    CHK_CONTINUOUS = 123,
    CHK_HUMANIZE = 124,

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

    TIMER_STATUS_CHECK = 500,
};

// Cached UI fonts (created in WinMain, destroyed at exit)
struct UiFonts {
    HFONT wordmark = nullptr;  // bold brand
    HFONT cardTitle = nullptr; // small uppercase section label
    HFONT button = nullptr;    // button labels
    HFONT body = nullptr;      // normal text
    HFONT value = nullptr;     // edit-field values
    HFONT pill = nullptr;      // status pill
    HFONT small_ = nullptr;    // muted captions
} g_fonts;

// DPI scale factor (1.0 at 96 DPI). All layout constants above are design units
// at 96 DPI; multiply through Sc()/Scf() so the UI scales on high-DPI monitors.
static double g_scale = 1.0;
static inline int   Sc(int v)    { return (int)(v * g_scale + 0.5); }
static inline float Scf(float v) { return (float)(v * g_scale); }

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
    UINT hotkeyRecord = VK_F8;
    UINT hotkeyPlayback = 'P';
    UINT hotkeyClicker = VK_F6;
    UINT hotkeyStop = VK_PAUSE;
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;  // For playback
} g_app;

// Temporary hotkey storage for customization dialog
UINT g_tempHotkeyRecord = VK_F8;
UINT g_tempHotkeyPlayback = 'P';
UINT g_tempHotkeyClicker = VK_F6;
UINT g_tempHotkeyStop = VK_PAUSE;

// Hotkey edit control handles
HWND g_hHotkeyRecordEdit = nullptr;
HWND g_hHotkeyPlaybackEdit = nullptr;
HWND g_hHotkeyClickerEdit = nullptr;
HWND g_hHotkeyStopEdit = nullptr;

// ============================================================================
// SETTINGS PERSISTENCE
// ============================================================================

std::string GetSettingsPath() {
    const char* appdata = getenv("APPDATA");
    std::string dir = appdata ? (std::string(appdata) + "\\FLOW") : std::string(".");
    CreateDirectoryA(dir.c_str(), NULL);
    return dir + "\\settings.cfg";
}

void SaveSettings() {
    std::ofstream f(GetSettingsPath());
    if (!f.is_open()) return;
    f << "playbackSpeed=" << g_app.playbackSpeed << "\n";
    f << "clickInterval=" << g_app.clickInterval << "\n";
    f << "loopCount=" << g_app.loopCount << "\n";
    f << "continuous=" << (g_app.continuous ? 1 : 0) << "\n";
    f << "alwaysOnTop=" << (g_app.alwaysOnTop ? 1 : 0) << "\n";
    f << "humanization=" << (g_app.humanizationEnabled ? 1 : 0) << "\n";
    f << "humanizationStdDev=" << g_app.humanizationStdDev << "\n";
    f << "hotkeyRecord=" << g_app.hotkeyRecord << "\n";
    f << "hotkeyPlayback=" << g_app.hotkeyPlayback << "\n";
    f << "hotkeyClicker=" << g_app.hotkeyClicker << "\n";
    f << "hotkeyStop=" << g_app.hotkeyStop << "\n";
}

void LoadSettings() {
    std::ifstream f(GetSettingsPath());
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        // Strip a trailing carriage return (in case of CRLF files)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (val.empty()) continue;

        if (key == "playbackSpeed") {
            double v = atof(val.c_str());
            if (v >= 0.1 && v <= 100.0) g_app.playbackSpeed = (float)v;
        } else if (key == "clickInterval") {
            int v = atoi(val.c_str());
            if (v >= 1 && v <= 10000) g_app.clickInterval = v;
        } else if (key == "loopCount") {
            int v = atoi(val.c_str());
            if (v >= 1 && v <= 999) g_app.loopCount = v;
        } else if (key == "continuous") {
            g_app.continuous = (atoi(val.c_str()) != 0);
        } else if (key == "alwaysOnTop") {
            g_app.alwaysOnTop = (atoi(val.c_str()) != 0);
        } else if (key == "humanization") {
            g_app.humanizationEnabled = (atoi(val.c_str()) != 0);
        } else if (key == "humanizationStdDev") {
            double v = atof(val.c_str());
            if (v >= 0.0 && v <= 1000.0) g_app.humanizationStdDev = v;
        } else if (key == "hotkeyRecord") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyRecord = v;
        } else if (key == "hotkeyPlayback") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyPlayback = v;
        } else if (key == "hotkeyClicker") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyClicker = v;
        } else if (key == "hotkeyStop") {
            UINT v = (UINT)atoi(val.c_str());
            if (v != 0) g_app.hotkeyStop = v;
        }
    }
}

// ============================================================================
// GDI+ DRAWING HELPERS  (anti-aliased)
// ============================================================================

static inline Gdiplus::Color GP(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void AddRoundRectPath(Gdiplus::GraphicsPath& path, float x, float y,
                             float w, float h, float r) {
    float d = r * 2.0f;
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static void FillRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
                      float r, Gdiplus::Color fill) {
    Gdiplus::GraphicsPath path;
    AddRoundRectPath(path, x, y, w, h, r);
    Gdiplus::SolidBrush brush(fill);
    g.FillPath(&brush, &path);
}

static void StrokeRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
                        float r, Gdiplus::Color stroke, float width = 1.0f) {
    Gdiplus::GraphicsPath path;
    // Inset by half the pen width so the stroke stays inside the rect
    AddRoundRectPath(path, x + width / 2, y + width / 2, w - width, h - width, r);
    Gdiplus::Pen pen(stroke, width);
    g.DrawPath(&pen, &path);
}

// Draw a white card with a soft drop shadow and a hairline border.
static void DrawCard(Gdiplus::Graphics& g, int x, int y, int w, int h) {
    float r = Scf((float)CARD_RADIUS);
    // Soft shadow: a few translucent offset rounds
    for (int i = 3; i >= 1; --i) {
        BYTE a = (BYTE)(10 + (3 - i) * 6);
        FillRound(g, (float)(x - i + 1), (float)(y + i), (float)(w + 2 * i - 2),
                  (float)(h + i), r + i, GP(SHADOW_COLOR, a));
    }
    FillRound(g, (float)x, (float)y, (float)w, (float)h, r, GP(BG_ELEVATED));
    StrokeRound(g, (float)x, (float)y, (float)w, (float)h, r, GP(BORDER_DEFAULT), Scf(1.0f));
}

// ---- Vector glyphs (centered at cx,cy, half-extent s) ----
static void IconCircle(Gdiplus::Graphics& g, float cx, float cy, float r, Gdiplus::Color c) {
    Gdiplus::SolidBrush b(c);
    g.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}
static void IconTriangle(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(cx - s * 0.55f, cy - s),
        Gdiplus::PointF(cx - s * 0.55f, cy + s),
        Gdiplus::PointF(cx + s * 0.9f,  cy)
    };
    Gdiplus::SolidBrush b(c);
    g.FillPolygon(&b, pts, 3);
}
static void IconSquare(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    FillRound(g, cx - s, cy - s, s * 2, s * 2, 3.0f, c);
}
static void IconBolt(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    float u = s / 8.0f;
    Gdiplus::PointF pts[6] = {
        Gdiplus::PointF(cx + 1.0f * u, cy - 8.0f * u),
        Gdiplus::PointF(cx - 4.0f * u, cy + 1.0f * u),
        Gdiplus::PointF(cx - 0.5f * u, cy + 1.0f * u),
        Gdiplus::PointF(cx - 1.5f * u, cy + 8.0f * u),
        Gdiplus::PointF(cx + 4.0f * u, cy - 2.0f * u),
        Gdiplus::PointF(cx + 0.5f * u, cy - 2.0f * u)
    };
    Gdiplus::SolidBrush b(c);
    g.FillPolygon(&b, pts, 6);
}

// ============================================================================
// OWNER-DRAW BUTTONS
// ============================================================================

HWND CreateFlowButton(HWND parent, int id, int x, int y, int w, int h, const wchar_t* tooltip) {
    HWND btn = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(LONG_PTR)id, GetModuleHandle(NULL), NULL);

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

enum class BtnIcon { None, Record, Play, Bolt, Stop };
enum class BtnStyle { Ghost, Filled };

// Render a single owner-draw button.
static void DrawFlowButton(DRAWITEMSTRUCT* dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    // Resolve per-button appearance
    const wchar_t* label = L"";
    BtnIcon icon = BtnIcon::None;
    BtnStyle style = BtnStyle::Ghost;
    COLORREF accent = ACCENT_PRIMARY;   // used for icon (ghost) or fill (filled)
    bool active = false;

    switch (dis->CtlID) {
        case BTN_RECORD:
            active = g_app.isRecording; accent = DANGER_COLOR; icon = BtnIcon::Record;
            label = active ? L"Recording" : L"Record"; break;
        case BTN_PLAY:
            active = g_app.isPlaying; accent = SUCCESS_COLOR; icon = BtnIcon::Play;
            label = active ? L"Playing" : L"Play"; break;
        case BTN_TOGGLE_CLICKER:
            active = g_app.isClicking; accent = ACCENT_PRIMARY; icon = BtnIcon::Bolt;
            label = active ? L"Clicking" : L"Start"; break;
        case BTN_STOP_ALL:
            style = BtnStyle::Filled; accent = DANGER_COLOR; icon = BtnIcon::Stop;
            label = L"Stop All"; active = true; break;
        case BTN_OPEN:   label = L"Open";     accent = ACCENT_PRIMARY; break;
        case BTN_SAVE:   label = L"Save";     accent = ACCENT_PRIMARY; break;
        case BTN_SETTINGS: label = L"Settings"; accent = TEXT_SECONDARY; break;
        default: break;
    }
    if (active) style = BtnStyle::Filled;

    COLORREF fillCol, textCol, iconCol, borderCol = BORDER_DEFAULT;
    bool drawBorder = false;
    if (style == BtnStyle::Filled) {
        fillCol = pressed ? ACCENT_PRESSED : accent;
        if (accent == DANGER_COLOR)  fillCol = pressed ? DANGER_COLOR  : DANGER_HOVER;
        if (accent == SUCCESS_COLOR) fillCol = pressed ? SUCCESS_COLOR : SUCCESS_HOVER;
        if (accent == ACCENT_PRIMARY)fillCol = pressed ? ACCENT_PRESSED: ACCENT_PRIMARY;
        textCol = RGB(255, 255, 255);
        iconCol = RGB(255, 255, 255);
    } else {
        fillCol = pressed ? TRACK_HOVER : TRACK_BG;
        textCol = TEXT_PRIMARY;
        iconCol = accent;
        drawBorder = true;
    }

    // The surface the button sits on (so the rounded corners blend in):
    // footer buttons are on the gray window, card buttons are on white cards.
    COLORREF behind = (dis->CtlID == BTN_OPEN || dis->CtlID == BTN_SAVE ||
                       dis->CtlID == BTN_SETTINGS || dis->CtlID == BTN_STOP_ALL)
                          ? BG_PRIMARY : BG_ELEVATED;

    // Background (anti-aliased)
    {
        HBRUSH bb = CreateSolidBrush(behind);
        FillRect(hdc, &rc, bb);
        DeleteObject(bb);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float br = Scf((float)BTN_RADIUS);
        FillRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(fillCol));
        if (drawBorder)
            StrokeRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(borderCol), Scf(1.0f));
    }

    // Measure label to center the icon + text group
    HFONT old = (HFONT)SelectObject(hdc, g_fonts.button);
    SIZE ts;
    GetTextExtentPoint32W(hdc, label, (int)wcslen(label), &ts);

    float iconW = (icon == BtnIcon::None) ? 0.0f : Scf(16.0f);
    float iconGap = (icon == BtnIcon::None) ? 0.0f : Scf(9.0f);
    float groupW = iconW + iconGap + ts.cx;
    float startX = (w - groupW) / 2.0f;
    float midY = h / 2.0f;

    if (icon != BtnIcon::None) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float icx = startX + iconW / 2.0f;
        Gdiplus::Color ic = GP(iconCol);
        switch (icon) {
            case BtnIcon::Record:   IconCircle(g, icx, midY, Scf(6.5f), ic); break;
            case BtnIcon::Play:     IconTriangle(g, icx, midY, Scf(7.5f), ic); break;
            case BtnIcon::Bolt:     IconBolt(g, icx, midY, Scf(9.0f), ic); break;
            case BtnIcon::Stop:     IconSquare(g, icx, midY, Scf(6.0f), ic); break;
            default: break;
        }
    }

    // Label
    RECT tr = rc;
    tr.left = (LONG)(startX + iconW + iconGap);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    DrawTextW(hdc, label, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, old);
}

// A centered, rounded dialog button (primary = filled accent, else ghost).
// Dialogs sit on a BG_PRIMARY surface.
static void DrawDlgButton(DRAWITEMSTRUCT* dis, const wchar_t* label, bool primary) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF fill, text; bool border = false;
    if (primary) {
        fill = pressed ? ACCENT_PRESSED : ACCENT_PRIMARY;
        text = RGB(255, 255, 255);
    } else {
        fill = pressed ? TRACK_HOVER : TRACK_BG;
        text = TEXT_PRIMARY;
        border = true;
    }

    HBRUSH bb = CreateSolidBrush(BG_PRIMARY);
    FillRect(hdc, &rc, bb);
    DeleteObject(bb);

    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float br = Scf((float)BTN_RADIUS);
        FillRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(fill));
        if (border)
            StrokeRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(BORDER_DEFAULT), Scf(1.0f));
    }

    HFONT old = (HFONT)SelectObject(hdc, g_fonts.button);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, old);
}

// ============================================================================
// STATUS DISPLAY
// ============================================================================

// The header pill and card contents are rendered in WM_PAINT, and the
// owner-draw buttons reflect record/play/click state, so a refresh invalidates
// the painted surface AND the child controls. (No erase => no flicker.)
void UpdateStatusDisplay() {
    if (g_app.hwnd)
        RedrawWindow(g_app.hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// ============================================================================
// SETTINGS MENU
// ============================================================================

// Speed / loops / interval / humanization / continuous now live inline on the
// cards. The menu only carries the occasional actions.
void ShowSettingsMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, MENU_CUSTOMIZE_HOTKEYS, L"Customize Hotkeys…");
    AppendMenuW(hMenu, MF_STRING | (g_app.alwaysOnTop ? MF_CHECKED : 0),
                MENU_ALWAYS_ON_TOP, L"Always on Top");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_CLEAR_RECORDING, L"Clear Recording");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, MENU_ABOUT, L"About FLOW");

    // Pop up just below the Settings button
    RECT rcBtn;
    GetWindowRect(GetDlgItem(hwnd, BTN_SETTINGS), &rcBtn);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, rcBtn.left, rcBtn.bottom + 4, 0, hwnd, NULL);
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
        } else {
            UpdateStatusDisplay();
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
        g_app.engine->SetPlaybackSpeed(g_app.playbackSpeed);
        if (g_app.continuous) {
            g_app.engine->StartPlayback(-1);
        } else {
            g_app.engine->StartPlayback(g_app.loopCount);
        }
        g_app.isPlaying = true;
        SetWindowTextA(hwnd, "FLOW - Playing...");
    }
    UpdateStatusDisplay();
    InvalidateRect(hwnd, NULL, TRUE);
}

void SetPlaybackSpeed(float speed) {
    g_app.playbackSpeed = speed;
    if (g_app.engine) {
        g_app.engine->SetPlaybackSpeed(speed);
    }
    UpdateStatusDisplay();
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
    RegisterHotKey(g_app.hwnd, HOTKEY_STOP, 0, g_app.hotkeyStop);
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
#define IDC_HOTKEY_RECORD 1007
#define IDC_HOTKEY_PLAYBACK 1008
#define IDC_HOTKEY_CLICKER 1009
#define IDC_HOTKEY_OK 1010
#define IDC_HOTKEY_CANCEL 1011

// Static IDs used only for muted-text coloring in the hotkey dialog
#define IDC_HK_SUBTITLE 9001
#define IDC_HK_INSTRUCTION 9002

// Hotkey dialog callback
LRESULT CALLBACK HotkeyDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            static HBRUSH bgB = CreateSolidBrush(BG_PRIMARY);
            static HBRUSH whiteB = CreateSolidBrush(BG_ELEVATED);
            HWND ctl = (HWND)lParam;
            int id = GetDlgCtrlID(ctl);
            HDC dc = (HDC)wParam;
            bool isKeyField = (id == IDC_HOTKEY_RECORD || id == IDC_HOTKEY_PLAYBACK ||
                               id == IDC_HOTKEY_CLICKER || id == IDC_HOTKEY_STOP);
            if (isKeyField) {                       // read-only key edits → white input
                SetTextColor(dc, TEXT_PRIMARY);
                SetBkColor(dc, BG_ELEVATED);
                return (INT_PTR)whiteB;
            }
            bool muted = (id == IDC_HK_SUBTITLE || id == IDC_HK_INSTRUCTION);
            SetTextColor(dc, muted ? TEXT_SECONDARY : TEXT_PRIMARY);
            SetBkColor(dc, BG_PRIMARY);
            return (INT_PTR)bgB;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == IDC_HOTKEY_OK)     { DrawDlgButton(dis, L"Save Changes", true);  return TRUE; }
            if (dis->CtlID == IDC_HOTKEY_CANCEL) { DrawDlgButton(dis, L"Cancel", false);        return TRUE; }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                case IDC_HOTKEY_OK: {
                    // Validate hotkeys
                    if (g_tempHotkeyRecord == 0 || g_tempHotkeyPlayback == 0 ||
                        g_tempHotkeyClicker == 0 || g_tempHotkeyStop == 0) {
                        MessageBoxA(hDlg, "Please set all hotkeys!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }

                    // Check for duplicates
                    UINT keys[] = {g_tempHotkeyRecord, g_tempHotkeyPlayback,
                                   g_tempHotkeyClicker, g_tempHotkeyStop};
                    for (int i = 0; i < 4; ++i) {
                        for (int j = i + 1; j < 4; ++j) {
                            if (keys[i] == keys[j]) {
                                MessageBoxA(hDlg, "Hotkeys must be unique!", "Error", MB_OK | MB_ICONERROR);
                                return 0;
                            }
                        }
                    }

                    // Unregister old hotkeys
                    UnregisterHotkeys();

                    // Apply new hotkeys
                    g_app.hotkeyRecord = g_tempHotkeyRecord;
                    g_app.hotkeyPlayback = g_tempHotkeyPlayback;
                    g_app.hotkeyClicker = g_tempHotkeyClicker;
                    g_app.hotkeyStop = g_tempHotkeyStop;

                    // Register new hotkeys
                    RegisterHotkeys();

                    DestroyWindow(hDlg);
                    break;
                }
                case IDCANCEL:
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
            g_hHotkeyStopEdit = NULL;
            break;
    }
    return 0;
}

LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    (void)dwRefData;
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
            } else if (hwnd == g_hHotkeyStopEdit) {
                g_tempHotkeyStop = vk;
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

    HINSTANCE hi = GetModuleHandle(NULL);
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_PRIMARY));

    // Layout in design units (scaled via Sc).
    const int W = 480, H = 452, padX = 28;
    const int rowTop = 104, rowH = 56, labelW = 168, editX = 200, editW = W - editX - padX, editH = 40;

    HWND title = CreateWindowExW(0, L"STATIC", L"Customize Hotkeys",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(20), Sc(W - 2 * padX), Sc(40),
        hDlg, NULL, hi, NULL);
    SendMessageW(title, WM_SETFONT, (WPARAM)g_fonts.wordmark, TRUE);

    HWND sub = CreateWindowExW(0, L"STATIC",
        L"Click a field, then press your key.  F1–F12, A–Z, 0–9.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(66), Sc(W - 2 * padX), Sc(24),
        hDlg, (HMENU)IDC_HK_SUBTITLE, hi, NULL);
    SendMessageW(sub, WM_SETFONT, (WPARAM)g_fonts.small_, TRUE);

    auto makeRow = [&](const wchar_t* text, UINT vk, bool mods, int editId, int y) -> HWND {
        HWND lab = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
            Sc(padX), Sc(y + 11), Sc(labelW), Sc(24), hDlg, NULL, hi, NULL);
        SendMessageW(lab, WM_SETFONT, (WPARAM)g_fonts.body, TRUE);
        const char* kn = GetKeyName(vk, mods);
        wchar_t wkn[64]; MultiByteToWideChar(CP_ACP, 0, kn, -1, wkn, 64);
        HWND e = CreateWindowExW(0, L"EDIT", wkn,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_READONLY | ES_CENTER,
            Sc(editX), Sc(y), Sc(editW), Sc(editH), hDlg, (HMENU)(LONG_PTR)editId, hi, NULL);
        SendMessageW(e, WM_SETFONT, (WPARAM)g_fonts.value, TRUE);
        SetWindowSubclass(e, HotkeyEditProc, 0, 0);
        return e;
    };

    g_hHotkeyRecordEdit   = makeRow(L"Start / stop recording", g_app.hotkeyRecord, false, IDC_HOTKEY_RECORD,   rowTop);
    g_hHotkeyPlaybackEdit = makeRow(L"Start / stop playback",  g_app.hotkeyPlayback, true, IDC_HOTKEY_PLAYBACK, rowTop + rowH);
    g_hHotkeyClickerEdit  = makeRow(L"Toggle auto-clicker",    g_app.hotkeyClicker, false, IDC_HOTKEY_CLICKER,  rowTop + 2 * rowH);
    g_hHotkeyStopEdit     = makeRow(L"Stop all activities",    g_app.hotkeyStop, false, IDC_HOTKEY_STOP,        rowTop + 3 * rowH);

    HWND instr = CreateWindowExW(0, L"STATIC",
        L"Playback also requires Ctrl+Shift+Alt. Changes apply on save.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(rowTop + 4 * rowH + 6), Sc(W - 2 * padX), Sc(24),
        hDlg, (HMENU)IDC_HK_INSTRUCTION, hi, NULL);
    SendMessageW(instr, WM_SETFONT, (WPARAM)g_fonts.small_, TRUE);

    const int btnY = 384, btnH = 44;
    HWND ok = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
        Sc(W - padX - 270), Sc(btnY), Sc(150), Sc(btnH), hDlg, (HMENU)IDC_HOTKEY_OK, hi, NULL);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        Sc(W - padX - 110), Sc(btnY), Sc(110), Sc(btnH), hDlg, (HMENU)IDC_HOTKEY_CANCEL, hi, NULL);
    (void)ok; (void)cancel;

    // Size the client area to exactly Sc(W) x Sc(H)
    SetWindowPos(hDlg, NULL, 0, 0, Sc(W), Sc(H), SWP_NOMOVE | SWP_NOZORDER);
    RECT rcC; GetClientRect(hDlg, &rcC);
    RECT rcW; GetWindowRect(hDlg, &rcW);
    SetWindowPos(hDlg, NULL, 0, 0,
        (rcW.right - rcW.left) + (Sc(W) - rcC.right),
        (rcW.bottom - rcW.top) + (Sc(H) - rcC.bottom),
        SWP_NOMOVE | SWP_NOZORDER);

    // Center on parent
    GetWindowRect(hDlg, &rcW);
    RECT rcParent; GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcW.right - rcW.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcW.bottom - rcW.top)) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

    ShowWindow(hDlg, SW_SHOW);
    SetFocus(g_hHotkeyRecordEdit);

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
}

// ============================================================================
// ABOUT DIALOG
// ============================================================================

#define IDC_ABOUT_OK   9100
#define IDC_ABOUT_SUB  9101
#define IDC_ABOUT_BODY 9102

LRESULT CALLBACK AboutDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            static HBRUSH bgB = CreateSolidBrush(BG_PRIMARY);
            int id = GetDlgCtrlID((HWND)lParam);
            HDC dc = (HDC)wParam;
            bool muted = (id == IDC_ABOUT_SUB || id == IDC_ABOUT_BODY);
            SetTextColor(dc, muted ? TEXT_SECONDARY : TEXT_PRIMARY);
            SetBkColor(dc, BG_PRIMARY);
            return (INT_PTR)bgB;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == IDC_ABOUT_OK) { DrawDlgButton(dis, L"Got it", true); return TRUE; }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                case IDCANCEL:
                case IDC_ABOUT_OK:
                    DestroyWindow(hDlg);
                    break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
    }
    return 0;
}

void ShowAboutDialog(HWND hwnd) {
    struct {
        DLGTEMPLATE dlg; WORD menu; WORD windowClass; WCHAR title[16];
        WORD fontSize; WCHAR fontName[16];
    } td;
    memset(&td, 0, sizeof(td));
    td.dlg.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER;
    td.dlg.cx = 200; td.dlg.cy = 140;
    wcscpy(td.title, L"About FLOW");
    td.fontSize = 9; wcscpy(td.fontName, L"Segoe UI");

    HWND hDlg = CreateDialogIndirectParamW(GetModuleHandle(NULL), &td.dlg, hwnd,
        (DLGPROC)AboutDialogWndProc, 0);
    if (!hDlg) return;

    HINSTANCE hi = GetModuleHandle(NULL);
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_PRIMARY));

    const int W = 440, H = 372, padX = 28;

    HWND title = CreateWindowExW(0, L"STATIC", L"FLOW", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Sc(padX), Sc(22), Sc(W - 2 * padX), Sc(40), hDlg, NULL, hi, NULL);
    SendMessageW(title, WM_SETFONT, (WPARAM)g_fonts.wordmark, TRUE);

    HWND sub = CreateWindowExW(0, L"STATIC", L"Flexible Low-latency Operations Workflow",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(64), Sc(W - 2 * padX), Sc(22),
        hDlg, (HMENU)IDC_ABOUT_SUB, hi, NULL);
    SendMessageW(sub, WM_SETFONT, (WPARAM)g_fonts.small_, TRUE);

    HWND body = CreateWindowExW(0, L"STATIC",
        L"A macro recorder, player, and high-speed auto-clicker.\r\n\r\n"
        L"Default hotkeys\r\n"
        L"F8  —  Start / stop recording\r\n"
        L"Ctrl+Shift+Alt+P  —  Start / stop playback\r\n"
        L"F6  —  Toggle auto-clicker\r\n"
        L"Pause  —  Stop everything\r\n\r\n"
        L"Settings are saved between sessions.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(100), Sc(W - 2 * padX), Sc(180),
        hDlg, (HMENU)IDC_ABOUT_BODY, hi, NULL);
    SendMessageW(body, WM_SETFONT, (WPARAM)g_fonts.body, TRUE);

    HWND ok = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
        Sc(W - padX - 120), Sc(300), Sc(120), Sc(44), hDlg, (HMENU)IDC_ABOUT_OK, hi, NULL);
    (void)ok;

    SetWindowPos(hDlg, NULL, 0, 0, Sc(W), Sc(H), SWP_NOMOVE | SWP_NOZORDER);
    RECT rcC; GetClientRect(hDlg, &rcC);
    RECT rcW; GetWindowRect(hDlg, &rcW);
    SetWindowPos(hDlg, NULL, 0, 0,
        (rcW.right - rcW.left) + (Sc(W) - rcC.right),
        (rcW.bottom - rcW.top) + (Sc(H) - rcC.bottom),
        SWP_NOMOVE | SWP_NOZORDER);

    GetWindowRect(hDlg, &rcW);
    RECT rcParent; GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcW.right - rcW.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcW.bottom - rcW.top)) / 2;
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

    ShowWindow(hDlg, SW_SHOW);

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
}


// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

static void TextLine(HDC hdc, const wchar_t* s, int x, int y, HFONT font, COLORREF color) {
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, s, (int)wcslen(s));
    SelectObject(hdc, old);
}

static void ApplySpeedEdit(HWND hwnd) {
    wchar_t buf[32];
    GetDlgItemTextW(hwnd, EDIT_SPEED, buf, 32);
    double v = wcstod(buf, nullptr);
    if (v < 0.1) v = 0.1;
    if (v > 100.0) v = 100.0;
    SetPlaybackSpeed((float)v);
    wchar_t out[32];
    swprintf(out, 32, L"%g", v);
    SetDlgItemTextW(hwnd, EDIT_SPEED, out);
}

static void ApplyLoopsEdit(HWND hwnd) {
    wchar_t buf[32];
    GetDlgItemTextW(hwnd, EDIT_LOOPS, buf, 32);
    int v = (int)wcstol(buf, nullptr, 10);
    if (v < 1) v = 1;
    if (v > 999) v = 999;
    g_app.loopCount = v;
    wchar_t out[16];
    swprintf(out, 16, L"%d", v);
    SetDlgItemTextW(hwnd, EDIT_LOOPS, out);
    UpdateStatusDisplay();
}

static void ApplyIntervalEdit(HWND hwnd) {
    wchar_t buf[32];
    GetDlgItemTextW(hwnd, EDIT_INTERVAL, buf, 32);
    int v = (int)wcstol(buf, nullptr, 10);
    if (v < 1) v = 1;
    if (v > 10000) v = 10000;
    g_app.clickInterval = v;
    if (g_app.engine && g_app.isClicking) g_app.engine->SetClickInterval(v);
    wchar_t out[16];
    swprintf(out, 16, L"%d", v);
    SetDlgItemTextW(hwnd, EDIT_INTERVAL, out);
    UpdateStatusDisplay();
}

// Render the whole window (header + cards + labels) into a device context.
static void PaintUI(HDC hdc, RECT client) {
    HBRUSH bg = CreateSolidBrush(BG_PRIMARY);
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    const wchar_t* stTxt; COLORREF stCol;
    if (g_app.isRecording)     { stTxt = L"Recording"; stCol = DANGER_COLOR; }
    else if (g_app.isPlaying)  { stTxt = L"Playing";   stCol = SUCCESS_COLOR; }
    else if (g_app.isClicking) { stTxt = L"Clicking";  stCol = ACCENT_PRIMARY; }
    else                       { stTxt = L"Ready";     stCol = TEXT_SECONDARY; }

    HFONT oldf = (HFONT)SelectObject(hdc, g_fonts.pill);
    SIZE pillTs; GetTextExtentPoint32W(hdc, stTxt, (int)wcslen(stTxt), &pillTs);
    SelectObject(hdc, oldf);
    int pillPadL = Sc(14), pillDot = Sc(8), pillGap = Sc(8), pillPadR = Sc(16), pillH = Sc(28);
    int pillW = pillPadL + pillDot + pillGap + pillTs.cx + pillPadR;
    int pillX = Sc(CLIENT_W) - Sc(PAD) - pillW;
    int pillY = (Sc(HEADER_H) - pillH) / 2 + Sc(2);

    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen divider(GP(BORDER_DEFAULT));
        g.DrawLine(&divider, (float)Sc(PAD), (float)Sc(HEADER_H), (float)(Sc(CLIENT_W) - Sc(PAD)), (float)Sc(HEADER_H));
        DrawCard(g, Sc(LEFT_X),  Sc(REC_TOP), Sc(COL_W), Sc(REC_H));
        DrawCard(g, Sc(LEFT_X),  Sc(CLK_TOP), Sc(COL_W), Sc(CLK_H));
        DrawCard(g, Sc(RIGHT_X), Sc(REC_TOP), Sc(COL_W), Sc(PLAY_H));
        FillRound(g, (float)pillX, (float)pillY, (float)pillW, (float)pillH, pillH / 2.0f, GP(stCol, 30));
        IconCircle(g, pillX + pillPadL + pillDot / 2.0f, pillY + pillH / 2.0f, pillDot / 2.0f, GP(stCol));
    }

    int ix = Sc(LEFT_X + 18);
    int pix = Sc(RIGHT_X + 18);

    TextLine(hdc, L"FLOW", Sc(PAD), Sc(9), g_fonts.wordmark, TEXT_PRIMARY);
    TextLine(hdc, L"Macro recorder & auto-clicker", Sc(PAD), Sc(39), g_fonts.small_, TEXT_SECONDARY);

    {
        RECT tr = { pillX + pillPadL + pillDot + pillGap, pillY, pillX + pillW, pillY + pillH };
        HFONT o = (HFONT)SelectObject(hdc, g_fonts.pill);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, stCol);
        DrawTextW(hdc, stTxt, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hdc, o);
    }

    TextLine(hdc, L"RECORD",       ix,  Sc(REC_TOP + 14), g_fonts.cardTitle, TEXT_FAINT);
    TextLine(hdc, L"AUTO-CLICKER", ix,  Sc(CLK_TOP + 14), g_fonts.cardTitle, TEXT_FAINT);
    TextLine(hdc, L"PLAYBACK",     pix, Sc(REC_TOP + 14), g_fonts.cardTitle, TEXT_FAINT);

    {
        wchar_t cap[64];
        unsigned long n = g_app.engine ? (unsigned long)g_app.engine->GetEventCount() : 0UL;
        if (n > 0) swprintf(cap, 64, L"%lu events captured", n);
        else       wcscpy(cap, L"No recording yet");
        TextLine(hdc, cap, ix, Sc(REC_TOP + REC_H - 30), g_fonts.small_, TEXT_SECONDARY);
    }

    TextLine(hdc, L"Interval (ms)", ix, Sc(CLK_TOP + 84), g_fonts.body, TEXT_SECONDARY);
    {
        wchar_t cps[48];
        int per = g_app.clickInterval > 0 ? g_app.clickInterval : 1;
        swprintf(cps, 48, L"≈ %d clicks/sec", (1000 + per / 2) / per);
        TextLine(hdc, cps, ix + Sc(110), Sc(CLK_TOP + 111), g_fonts.small_, TEXT_FAINT);
    }

    TextLine(hdc, L"Speed", pix, Sc(REC_TOP + 88),  g_fonts.body, TEXT_SECONDARY);
    TextLine(hdc, L"×", pix + Sc(100), Sc(REC_TOP + 112), g_fonts.body, TEXT_SECONDARY);
    TextLine(hdc, L"Loops", pix, Sc(REC_TOP + 150), g_fonts.body, TEXT_SECONDARY);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;  // background painted in WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
            PaintUI(mem, rc);
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            static HBRUSH white = CreateSolidBrush(BG_ELEVATED);
            HDC dc = (HDC)wParam;
            SetTextColor(dc, TEXT_PRIMARY);
            SetBkColor(dc, BG_ELEVATED);
            return (INT_PTR)white;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (code == EN_KILLFOCUS) {
                if (id == EDIT_SPEED) ApplySpeedEdit(hwnd);
                else if (id == EDIT_LOOPS) ApplyLoopsEdit(hwnd);
                else if (id == EDIT_INTERVAL) ApplyIntervalEdit(hwnd);
                break;
            }

            switch (id) {
                case BTN_OPEN: OpenMacroFile(hwnd); break;
                case BTN_SAVE: SaveMacroFile(hwnd); break;
                case BTN_RECORD: ToggleRecording(hwnd); break;
                case BTN_PLAY: TogglePlayback(hwnd); break;
                case BTN_STOP_ALL: StopAll(hwnd); break;
                case BTN_SETTINGS: ShowSettingsMenu(hwnd); break;
                case BTN_TOGGLE_CLICKER: ToggleAutoClicker(); break;

                case CHK_CONTINUOUS:
                    g_app.continuous =
                        (SendDlgItemMessageW(hwnd, CHK_CONTINUOUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    UpdateStatusDisplay();
                    break;

                case CHK_HUMANIZE:
                    g_app.humanizationEnabled =
                        (SendDlgItemMessageW(hwnd, CHK_HUMANIZE, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (g_app.engine) {
                        g_app.engine->EnableHumanization(g_app.humanizationEnabled);
                        if (g_app.humanizationEnabled)
                            g_app.engine->ConfigureHumanization(0.0, g_app.humanizationStdDev);
                    }
                    break;

                case MENU_CUSTOMIZE_HOTKEYS: ShowCustomizeHotkeysDialog(hwnd); break;
                case MENU_ALWAYS_ON_TOP:
                    g_app.alwaysOnTop = !g_app.alwaysOnTop;
                    UpdateWindowState();
                    break;
                case MENU_CLEAR_RECORDING:
                    if (g_app.engine) g_app.engine->ClearRecording();
                    UpdateStatusDisplay();
                    break;
                case MENU_ABOUT:
                    ShowAboutDialog(hwnd);
                    break;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                DrawFlowButton(dis);
                return TRUE;
            }
            break;
        }

        case WM_TIMER:
            if (wParam == TIMER_STATUS_CHECK) {
                if (g_app.isPlaying && g_app.engine && !g_app.engine->IsPlaybackActive()) {
                    g_app.isPlaying = false;
                    UpdateStatusDisplay();
                }
            }
            break;

        case WM_HOTKEY:
            if (wParam == HOTKEY_RECORD) ToggleRecording(hwnd);
            else if (wParam == HOTKEY_PLAYBACK) TogglePlayback(hwnd);
            else if (wParam == HOTKEY_CLICKER) ToggleAutoClicker();
            else if (wParam == HOTKEY_STOP) StopAll(hwnd);
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            SaveSettings();
            KillTimer(hwnd, TIMER_STATUS_CHECK);
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
    HINSTANCE hi = GetModuleHandle(NULL);
    int ix = Sc(LEFT_X + 18);
    int pix = Sc(RIGHT_X + 18);
    int innerW = Sc(COL_W - 36);
    int eW = Sc(92), eH = Sc(28);

    // Primary action buttons (owner-draw)
    CreateFlowButton(hwnd, BTN_RECORD, ix, Sc(REC_TOP + 38), innerW, Sc(42), L"Start / stop recording (F8)");
    CreateFlowButton(hwnd, BTN_PLAY, pix, Sc(REC_TOP + 38), innerW, Sc(42), L"Start / stop playback (Ctrl+Shift+Alt+P)");
    CreateFlowButton(hwnd, BTN_TOGGLE_CLICKER, ix, Sc(CLK_TOP + 38), innerW, Sc(42), L"Toggle auto-clicker (F6)");

    // Inline edit fields
    auto makeEdit = [&](int id, int x, int y, DWORD extra) -> HWND {
        HWND e = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER | extra,
            x, y, eW, eH, hwnd, (HMENU)(LONG_PTR)id, hi, NULL);
        SendMessageW(e, WM_SETFONT, (WPARAM)g_fonts.value, TRUE);
        return e;
    };
    makeEdit(EDIT_SPEED,    pix, Sc(REC_TOP + 108), 0);
    makeEdit(EDIT_LOOPS,    pix, Sc(REC_TOP + 170), ES_NUMBER);
    makeEdit(EDIT_INTERVAL, ix,  Sc(CLK_TOP + 106), ES_NUMBER);

    // Checkboxes
    HWND c1 = CreateWindowExW(0, L"BUTTON", L"Loop continuously",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        pix, Sc(REC_TOP + 210), Sc(220), Sc(22), hwnd, (HMENU)CHK_CONTINUOUS, hi, NULL);
    SendMessageW(c1, WM_SETFONT, (WPARAM)g_fonts.body, TRUE);

    HWND c2 = CreateWindowExW(0, L"BUTTON", L"Humanize timing",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        pix, Sc(REC_TOP + 236), Sc(220), Sc(22), hwnd, (HMENU)CHK_HUMANIZE, hi, NULL);
    SendMessageW(c2, WM_SETFONT, (WPARAM)g_fonts.body, TRUE);

    // Footer buttons
    CreateFlowButton(hwnd, BTN_OPEN, Sc(LEFT_X), Sc(FOOTER_TOP), Sc(120), Sc(42), L"Open a saved macro (.rec)");
    CreateFlowButton(hwnd, BTN_SAVE, Sc(LEFT_X + 128), Sc(FOOTER_TOP), Sc(120), Sc(42), L"Save the current macro");
    CreateFlowButton(hwnd, BTN_SETTINGS, Sc(314), Sc(FOOTER_TOP), Sc(110), Sc(42), L"Hotkeys, always-on-top, about");
    CreateFlowButton(hwnd, BTN_STOP_ALL, Sc(432), Sc(FOOTER_TOP), Sc(CLIENT_W - PAD - 432), Sc(42), L"Stop everything (Pause)");

    // Initialize control values from state
    wchar_t buf[32];
    swprintf(buf, 32, L"%g", g_app.playbackSpeed); SetDlgItemTextW(hwnd, EDIT_SPEED, buf);
    swprintf(buf, 32, L"%d", g_app.loopCount);      SetDlgItemTextW(hwnd, EDIT_LOOPS, buf);
    swprintf(buf, 32, L"%d", g_app.clickInterval);  SetDlgItemTextW(hwnd, EDIT_INTERVAL, buf);
    SendDlgItemMessageW(hwnd, CHK_CONTINUOUS, BM_SETCHECK, g_app.continuous ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(hwnd, CHK_HUMANIZE,   BM_SETCHECK, g_app.humanizationEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // High-DPI awareness for crisp rendering
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        auto setDpiFunc = (SetProcessDpiAwarenessContextFunc)(void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpiFunc) setDpiFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        FreeLibrary(user32);
    }

    flow::protection::EnforceProtection();

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Determine the DPI scale factor for the primary monitor so the whole UI
    // scales up on high-DPI displays instead of rendering tiny.
    {
        HDC screen = GetDC(NULL);
        int dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(NULL, screen);
        if (dpi > 0) g_scale = dpi / 96.0;
        if (g_scale < 1.0) g_scale = 1.0;
    }

    auto mkFont = [](int h, int weight, bool italic = false) -> HFONT {
        return CreateFontW(h, 0, 0, 0, weight, italic, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    };
    g_fonts.wordmark  = mkFont(Sc(30), FW_BOLD);
    g_fonts.cardTitle = mkFont(Sc(13), FW_BOLD);
    g_fonts.button    = mkFont(Sc(17), FW_SEMIBOLD);
    g_fonts.body      = mkFont(Sc(16), FW_NORMAL);
    g_fonts.value     = mkFont(Sc(17), FW_SEMIBOLD);
    g_fonts.pill      = mkFont(Sc(15), FW_SEMIBOLD);
    g_fonts.small_    = mkFont(Sc(14), FW_NORMAL);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;  // painted in WM_PAINT
    wc.lpszClassName = L"FLOW_Modern";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window registration failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT wr = { 0, 0, Sc(CLIENT_W), Sc(CLIENT_H) };
    // Use the DPI-aware frame calculation when available so the client area is
    // exactly the size we lay out for (AdjustWindowRect uses 96-DPI metrics).
    typedef BOOL(WINAPI* AdjustForDpiFunc)(LPRECT, DWORD, BOOL, DWORD, UINT);
    HMODULE u32 = LoadLibraryA("user32.dll");
    AdjustForDpiFunc adjustForDpi = u32 ?
        (AdjustForDpiFunc)(void*)GetProcAddress(u32, "AdjustWindowRectExForDpi") : nullptr;
    if (adjustForDpi) adjustForDpi(&wr, style, FALSE, 0, (UINT)(96 * g_scale + 0.5));
    else              AdjustWindowRect(&wr, style, FALSE);
    if (u32) FreeLibrary(u32);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;

    g_app.hwnd = CreateWindowExW(0, L"FLOW_Modern", L"FLOW",
        style,
        (GetSystemMetrics(SM_CXSCREEN) - winW) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - winH) / 2,
        winW, winH, NULL, NULL, hInstance, NULL);

    if (!g_app.hwnd) {
        MessageBoxW(NULL, L"Window creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_app.engine = new FlowEngine();

    // Restore persisted settings, then apply to engine + UI
    LoadSettings();
    g_app.engine->SetPlaybackSpeed(g_app.playbackSpeed);
    g_app.engine->EnableHumanization(g_app.humanizationEnabled);
    g_app.engine->ConfigureHumanization(0.0, g_app.humanizationStdDev);

    if (!g_app.engine->InstallHooks()) {
        MessageBoxW(NULL, L"Failed to install input hooks!\n\nRun as Administrator.",
            L"Error", MB_OK | MB_ICONERROR);
        delete g_app.engine;
        return 1;
    }

    RegisterHotkeys();
    CreateControls(g_app.hwnd);
    UpdateWindowState();

    SetTimer(g_app.hwnd, TIMER_STATUS_CHECK, 100, NULL);
    ShowWindow(g_app.hwnd, nCmdShow);
    UpdateWindow(g_app.hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(g_app.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnregisterHotkeys();
    KillTimer(g_app.hwnd, TIMER_STATUS_CHECK);
    g_app.engine->UninstallHooks();
    delete g_app.engine;

    DeleteObject(g_fonts.wordmark);
    DeleteObject(g_fonts.cardTitle);
    DeleteObject(g_fonts.button);
    DeleteObject(g_fonts.body);
    DeleteObject(g_fonts.value);
    DeleteObject(g_fonts.pill);
    DeleteObject(g_fonts.small_);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
