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

// Layout (client-area design units at 96 DPI, single-column workflow).
constexpr int PAD        = 24;
constexpr int CLIENT_W   = 460;
constexpr int CLIENT_H   = 698;
constexpr int CONTENT_X  = PAD;                 // 24
constexpr int CONTENT_W  = CLIENT_W - 2 * PAD;  // 412
constexpr int CRIGHT     = CLIENT_W - PAD;      // 436 (right edge of content)
constexpr int BTN_RADIUS = 10;
constexpr int HERO_H     = 48;                  // primary (hero) button height
constexpr int SEC_BTN_H  = 44;                  // secondary (auto-clicker) button
constexpr int FOOT_H     = 40;                  // footer button height
constexpr int EDIT_W     = 52;
constexpr int CTRL_W     = CONTENT_W - 16;      // right-aligned control width (toggles)

// Vertical rhythm — explicit Y of every element (PaintUI + CreateControls share these)
constexpr int HEADER_Y     = 24;

constexpr int REC_DIV_Y    = 56;
constexpr int REC_LABEL_Y  = 72;
constexpr int REC_BTN_Y    = 92;
constexpr int REC_INFO1_Y  = 152;
constexpr int REC_INFO2_Y  = 174;

constexpr int PLAY_DIV_Y   = 200;
constexpr int PLAY_LABEL_Y = 216;
constexpr int PLAY_BTN_Y   = 236;
constexpr int ROW_SPEED_Y  = 298;   // label/control row baselines
constexpr int ROW_LOOPS_Y  = 334;
constexpr int ROW_CONT_Y   = 370;
constexpr int ROW_HUM_Y    = 404;
constexpr int RUNTIME_Y    = 440;

constexpr int CLK_DIV_Y     = 466;
constexpr int CLK_LABEL_Y   = 482;
constexpr int CLK_CAPTION_Y = 500;
constexpr int CLK_BTN_Y     = 522;
constexpr int ROW_INTERVAL_Y= 582;

constexpr int FOOT_DIV_Y   = 620;
constexpr int FOOT_Y       = 634;

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
    HFONT pill = nullptr;      // status label
    HFONT small_ = nullptr;    // muted captions
    HFONT mono = nullptr;      // monospace numerics
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
    UINT hotkeyPlayback = VK_F9;
    UINT hotkeyClicker = VK_F6;
    UINT hotkeyStop = VK_PAUSE;
    UINT hotkeyModifiers = 0;  // optional modifiers for playback (none by default)
} g_app;

// Temporary hotkey storage for customization dialog
UINT g_tempHotkeyRecord = VK_F8;
UINT g_tempHotkeyPlayback = VK_F9;
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

static COLORREF Darken(COLORREF c, double f) {
    return RGB((int)(GetRValue(c) * f), (int)(GetGValue(c) * f), (int)(GetBValue(c) * f));
}

// Forward decl (defined in the ACTIONS section); used for on-button hotkey hints.
const char* GetKeyName(UINT vk, bool withModifiers);

static void DrawIcon(Gdiplus::Graphics& g, BtnIcon icon, float cx, float cy, Gdiplus::Color c) {
    switch (icon) {
        case BtnIcon::Record:   IconCircle(g, cx, cy, Scf(6.5f), c); break;
        case BtnIcon::Play:     IconTriangle(g, cx, cy, Scf(7.5f), c); break;
        case BtnIcon::Bolt:     IconBolt(g, cx, cy, Scf(9.0f), c); break;
        case BtnIcon::Stop:     IconSquare(g, cx, cy, Scf(6.0f), c); break;
        default: break;
    }
}

enum class Role { Hero, Secondary, Ghost, Disabled };

// Render an owner-draw button. Exactly one section action is the saturated
// "hero" at a time: Record (idle), Play (once a macro exists), or the active
// Stop. Play is disabled-grey with no macro; the auto-clicker is a demoted
// secondary (outline) until running; footer buttons are ghost.
static void DrawFlowButton(DRAWITEMSTRUCT* dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool hasMacro = g_app.engine && g_app.engine->GetEventCount() > 0;

    const wchar_t* label = L"";
    BtnIcon icon = BtnIcon::None;
    COLORREF accent = ACCENT_PRIMARY;
    bool active = false, leftAlign = false;
    wchar_t hint[64] = L"";
    Role role = Role::Ghost;

    switch (dis->CtlID) {
        case BTN_RECORD:
            leftAlign = true; accent = DANGER_COLOR; icon = BtnIcon::Record;
            active = g_app.isRecording; role = Role::Hero;
            label = active ? L"Stop Recording" : L"Record";
            MultiByteToWideChar(CP_ACP, 0, GetKeyName(g_app.hotkeyRecord, false), -1, hint, 64); break;
        case BTN_PLAY:
            leftAlign = true; accent = SUCCESS_COLOR; icon = BtnIcon::Play;
            active = g_app.isPlaying;
            role = (disabled || !hasMacro) ? Role::Disabled : Role::Hero;
            label = active ? L"Stop Playback" : L"Play";
            MultiByteToWideChar(CP_ACP, 0, GetKeyName(g_app.hotkeyPlayback, true), -1, hint, 64); break;
        case BTN_TOGGLE_CLICKER:
            leftAlign = true; accent = ACCENT_PRIMARY; icon = BtnIcon::Bolt;
            active = g_app.isClicking; role = active ? Role::Hero : Role::Secondary;
            label = active ? L"Stop Clicking" : L"Start Clicking";
            MultiByteToWideChar(CP_ACP, 0, GetKeyName(g_app.hotkeyClicker, false), -1, hint, 64); break;
        case BTN_STOP_ALL:
            active = (g_app.isRecording || g_app.isPlaying || g_app.isClicking);
            accent = DANGER_COLOR; label = L"Stop All";
            if (active) { role = Role::Hero; icon = BtnIcon::Stop; }
            else        { role = Role::Ghost; icon = BtnIcon::None; }
            break;
        case BTN_OPEN:     label = L"Open"; role = Role::Ghost; break;
        case BTN_SAVE:     label = L"Save"; role = Role::Ghost; break;
        case BTN_SETTINGS: label = L"Settings"; role = Role::Ghost; break;
        default: break;
    }

    COLORREF fillCol, textCol, iconCol, borderCol = BORDER_DEFAULT, hintCol = TEXT_FAINT;
    float borderW = 0.0f;
    switch (role) {
        case Role::Hero:
            fillCol = pressed ? Darken(accent, 0.86) : accent;
            textCol = RGB(255, 255, 255); iconCol = RGB(255, 255, 255);
            hintCol = RGB(236, 241, 250); break;
        case Role::Secondary:
            fillCol = pressed ? TRACK_HOVER : BG_ELEVATED;
            textCol = accent; iconCol = accent;
            borderCol = accent; borderW = Scf(1.6f); hintCol = ACCENT_HOVER; break;
        case Role::Disabled:
            fillCol = TRACK_BG; textCol = TEXT_FAINT; iconCol = TEXT_FAINT; hintCol = TEXT_FAINT; break;
        case Role::Ghost:
        default:
            fillCol = pressed ? TRACK_HOVER : BG_ELEVATED;
            textCol = TEXT_PRIMARY; iconCol = TEXT_SECONDARY;
            borderCol = BORDER_DEFAULT; borderW = Scf(1.0f); break;
    }

    // Background
    {
        HBRUSH bb = CreateSolidBrush(BG_PRIMARY);
        FillRect(hdc, &rc, bb);
        DeleteObject(bb);
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float br = Scf((float)BTN_RADIUS);
        FillRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(fillCol));
        if (borderW > 0.0f)
            StrokeRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, br, GP(borderCol), borderW);
    }

    SetBkMode(hdc, TRANSPARENT);
    float midY = h / 2.0f;

    if (leftAlign) {
        if (icon != BtnIcon::None) {
            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            DrawIcon(g, icon, rc.left + Scf(24.0f), midY, GP(iconCol));
        }
        HFONT old = (HFONT)SelectObject(hdc, g_fonts.button);
        RECT tr = rc; tr.left = rc.left + Sc(44);
        SetTextColor(hdc, textCol);
        DrawTextW(hdc, label, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        if (hint[0]) {
            SelectObject(hdc, g_fonts.small_);
            SetTextColor(hdc, hintCol);
            RECT hr = rc; hr.right -= Sc(18);
            DrawTextW(hdc, hint, -1, &hr, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(hdc, old);
    } else {
        HFONT old = (HFONT)SelectObject(hdc, g_fonts.button);
        SIZE ts; GetTextExtentPoint32W(hdc, label, (int)wcslen(label), &ts);
        float iconW = (icon == BtnIcon::None) ? 0.0f : Scf(15.0f);
        float iconGap = (icon == BtnIcon::None) ? 0.0f : Scf(8.0f);
        float startX = (w - (iconW + iconGap + ts.cx)) / 2.0f;
        if (icon != BtnIcon::None) {
            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            DrawIcon(g, icon, startX + iconW / 2.0f, midY, GP(iconCol));
        }
        RECT tr = rc; tr.left = (LONG)(startX + iconW + iconGap);
        SetTextColor(hdc, textCol);
        DrawTextW(hdc, label, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hdc, old);
    }
}

// An on/off toggle switch with a label on the left and the switch on the right.
static void DrawToggle(DRAWITEMSTRUCT* dis, const wchar_t* label, bool on) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    HBRUSH bb = CreateSolidBrush(BG_PRIMARY);
    FillRect(hdc, &rc, bb);
    DeleteObject(bb);

    HFONT old = (HFONT)SelectObject(hdc, g_fonts.body);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_PRIMARY);
    RECT tr = rc;
    DrawTextW(hdc, label, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, old);

    float tw = Scf(42.0f), th = Scf(24.0f);
    float tx = rc.left + w - tw;
    float ty = (h - th) / 2.0f;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    FillRound(g, tx, ty, tw, th, th / 2.0f, GP(on ? ACCENT_PRIMARY : RGB(203, 213, 225)));
    float kr = th / 2.0f - Scf(3.0f);
    float ky = ty + th / 2.0f;
    float kx = on ? (tx + tw - th / 2.0f) : (tx + th / 2.0f);
    Gdiplus::SolidBrush knob(GP(RGB(255, 255, 255)));
    g.FillEllipse(&knob, kx - kr, ky - kr, kr * 2, kr * 2);
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
    if (!g_app.hwnd) return;
    // Play is only available once a macro exists.
    HWND play = GetDlgItem(g_app.hwnd, BTN_PLAY);
    if (play) EnableWindow(play, g_app.engine && g_app.engine->GetEventCount() > 0);
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
        L"F9  —  Start / stop playback\r\n"
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

    const int cx = Sc(CONTENT_X);
    const int cright = Sc(CRIGHT);

    // ---- status state (idle uses a visible muted slate; real color when active) ----
    const wchar_t* stTxt; COLORREF stDot, stTextCol;
    if (g_app.isRecording)     { stTxt = L"Recording"; stDot = DANGER_COLOR;   stTextCol = DANGER_COLOR; }
    else if (g_app.isPlaying)  { stTxt = L"Playing";   stDot = SUCCESS_COLOR;  stTextCol = SUCCESS_COLOR; }
    else if (g_app.isClicking) { stTxt = L"Clicking";  stDot = ACCENT_PRIMARY; stTextCol = ACCENT_PRIMARY; }
    else                       { stTxt = L"Ready";     stDot = TEXT_SECONDARY; stTextCol = TEXT_SECONDARY; }

    HFONT of = (HFONT)SelectObject(hdc, g_fonts.pill);
    SIZE sts; GetTextExtentPoint32W(hdc, stTxt, (int)wcslen(stTxt), &sts);
    SelectObject(hdc, of);
    int dotR = Sc(5), dotGap = Sc(9);
    int stX = cright - (dotR * 2 + dotGap + sts.cx);
    int stCy = Sc(HEADER_Y) + Sc(8);

    // ---- dividers + status dot ----
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen div(GP(BORDER_DEFAULT), Scf(1.0f));
        int ys[] = { Sc(REC_DIV_Y), Sc(PLAY_DIV_Y), Sc(CLK_DIV_Y), Sc(FOOT_DIV_Y) };
        for (int yy : ys) g.DrawLine(&div, (float)cx, (float)yy, (float)cright, (float)yy);
        IconCircle(g, (float)(stX + dotR), (float)stCy, (float)dotR, GP(stDot));
    }
    {
        HFONT o = (HFONT)SelectObject(hdc, g_fonts.pill);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, stTextCol);
        RECT tr = { stX + dotR * 2 + dotGap, stCy - Sc(14), cright, stCy + Sc(14) };
        DrawTextW(hdc, stTxt, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hdc, o);
    }

    // ---- soft input pills behind the borderless edits (accent ring on focus) ----
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        struct E { int id, x, y, w; };
        E edits[] = {
            { EDIT_SPEED,    368, ROW_SPEED_Y - 4,    EDIT_W },
            { EDIT_LOOPS,    368, ROW_LOOPS_Y - 4,    EDIT_W },
            { EDIT_INTERVAL, 110, ROW_INTERVAL_Y - 4, 70 },
        };
        HWND focus = GetFocus();
        for (const E& e : edits) {
            float px = Sc(e.x) - Scf(5), py = Sc(e.y) - Scf(3);
            float pw = Sc(e.w) + Scf(10), ph = Sc(30) + Scf(6);
            FillRound(g, px, py, pw, ph, Scf(8.0f), GP(TRACK_BG));
            if (g_app.hwnd && GetDlgItem(g_app.hwnd, e.id) == focus)
                StrokeRound(g, px, py, pw, ph, Scf(8.0f), GP(ACCENT_PRIMARY), Scf(1.6f));
        }
    }

    // ---- header (no duplicate wordmark; chrome title carries the name) ----
    TextLine(hdc, L"Macro recorder & auto-clicker", cx, Sc(HEADER_Y), g_fonts.button, TEXT_PRIMARY);

    // ---- section labels ----
    TextLine(hdc, L"RECORD",       cx, Sc(REC_LABEL_Y),  g_fonts.cardTitle, TEXT_FAINT);
    TextLine(hdc, L"PLAYBACK",     cx, Sc(PLAY_LABEL_Y), g_fonts.cardTitle, TEXT_FAINT);
    TextLine(hdc, L"AUTO-CLICKER", cx, Sc(CLK_LABEL_Y),  g_fonts.cardTitle, TEXT_FAINT);

    // ---- record info / empty state ----
    {
        unsigned long n = g_app.engine ? (unsigned long)g_app.engine->GetEventCount() : 0UL;
        if (n > 0) {
            DWORD ms = g_app.engine->GetDurationMs();
            double s = ms / 1000.0;
            wchar_t dur[24];
            if (s < 60.0) swprintf(dur, 24, L"%.1fs", s);
            else          swprintf(dur, 24, L"%dm %02ds", (int)(s / 60), (int)s % 60);
            wchar_t cap[64];
            swprintf(cap, 64, L"%lu events   ·   %ls recorded", n, dur);
            TextLine(hdc, cap, cx, Sc(REC_INFO1_Y), g_fonts.mono, TEXT_SECONDARY);
        } else {
            wchar_t hint[96];
            char kn[32]; strcpy(kn, GetKeyName(g_app.hotkeyRecord, false));
            wchar_t wkn[32]; MultiByteToWideChar(CP_ACP, 0, kn, -1, wkn, 32);
            swprintf(hint, 96, L"Press %ls to record, or open a saved .rec file.", wkn);
            TextLine(hdc, L"No macro loaded", cx, Sc(REC_INFO1_Y), g_fonts.body, TEXT_PRIMARY);
            TextLine(hdc, hint, cx, Sc(REC_INFO2_Y), g_fonts.small_, TEXT_FAINT);
        }
    }

    // ---- playback rows (labels left; controls are right-aligned child widgets) ----
    TextLine(hdc, L"Speed", cx, Sc(ROW_SPEED_Y), g_fonts.body, TEXT_SECONDARY);
    TextLine(hdc, L"×", Sc(424), Sc(ROW_SPEED_Y), g_fonts.body, TEXT_FAINT);
    TextLine(hdc, L"Loops", cx, Sc(ROW_LOOPS_Y), g_fonts.body, TEXT_SECONDARY);
    // (Loop continuously / Humanize labels are drawn by their toggle buttons.)

    // ---- runtime estimate ----
    if (g_app.engine && g_app.engine->GetEventCount() > 0) {
        double durS = g_app.engine->GetDurationMs() / 1000.0;
        double sp = g_app.playbackSpeed > 0 ? g_app.playbackSpeed : 1.0;
        wchar_t rt[64];
        if (g_app.continuous) {
            swprintf(rt, 64, L"Est. runtime   ·   continuous");
        } else {
            double tot = durS * g_app.loopCount / sp;
            if (tot < 60.0) swprintf(rt, 64, L"Est. runtime   ·   %.1fs", tot);
            else            swprintf(rt, 64, L"Est. runtime   ·   %dm %02ds", (int)(tot / 60), (int)tot % 60);
        }
        TextLine(hdc, rt, cx, Sc(RUNTIME_Y), g_fonts.small_, TEXT_FAINT);
    }

    // ---- auto-clicker (set apart: its own caption — works without a macro) ----
    TextLine(hdc, L"Spam clicks at a fixed rate — no macro needed.",
             cx, Sc(CLK_CAPTION_Y), g_fonts.small_, TEXT_FAINT);
    TextLine(hdc, L"Interval", cx, Sc(ROW_INTERVAL_Y), g_fonts.body, TEXT_SECONDARY);
    {
        wchar_t cps[48];
        int per = g_app.clickInterval > 0 ? g_app.clickInterval : 1;
        swprintf(cps, 48, L"ms    ≈ %d clicks/sec", (1000 + per / 2) / per);
        TextLine(hdc, cps, cx + Sc(182), Sc(ROW_INTERVAL_Y), g_fonts.small_, TEXT_FAINT);
    }
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
            static HBRUSH track = CreateSolidBrush(TRACK_BG);
            HDC dc = (HDC)wParam;
            SetTextColor(dc, TEXT_PRIMARY);
            SetBkColor(dc, TRACK_BG);
            return (INT_PTR)track;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (code == EN_KILLFOCUS) {
                if (id == EDIT_SPEED) ApplySpeedEdit(hwnd);
                else if (id == EDIT_LOOPS) ApplyLoopsEdit(hwnd);
                else if (id == EDIT_INTERVAL) ApplyIntervalEdit(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);   // clear focus ring
                break;
            }
            if (code == EN_SETFOCUS) {
                InvalidateRect(hwnd, NULL, FALSE);    // draw focus ring
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
                    g_app.continuous = !g_app.continuous;
                    InvalidateRect(GetDlgItem(hwnd, CHK_CONTINUOUS), NULL, FALSE);
                    UpdateStatusDisplay();   // runtime estimate depends on it
                    break;

                case CHK_HUMANIZE:
                    g_app.humanizationEnabled = !g_app.humanizationEnabled;
                    if (g_app.engine) {
                        g_app.engine->EnableHumanization(g_app.humanizationEnabled);
                        if (g_app.humanizationEnabled)
                            g_app.engine->ConfigureHumanization(0.0, g_app.humanizationStdDev);
                    }
                    InvalidateRect(GetDlgItem(hwnd, CHK_HUMANIZE), NULL, FALSE);
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
                if (dis->CtlID == CHK_CONTINUOUS)
                    DrawToggle(dis, L"Loop continuously", g_app.continuous);
                else if (dis->CtlID == CHK_HUMANIZE)
                    DrawToggle(dis, L"Humanize timing", g_app.humanizationEnabled);
                else
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
    int cx = Sc(CONTENT_X);
    int contentW = Sc(CONTENT_W);

    // Section action buttons (visual weight resolved per-state in DrawFlowButton)
    CreateFlowButton(hwnd, BTN_RECORD, cx, Sc(REC_BTN_Y), contentW, Sc(HERO_H), L"Start / stop recording");
    CreateFlowButton(hwnd, BTN_PLAY, cx, Sc(PLAY_BTN_Y), contentW, Sc(HERO_H), L"Play the recorded macro");
    CreateFlowButton(hwnd, BTN_TOGGLE_CLICKER, cx, Sc(CLK_BTN_Y), contentW, Sc(SEC_BTN_H), L"Start / stop the auto-clicker");

    // Inline numeric edits (monospace, borderless — focus ring drawn in PaintUI)
    auto makeEdit = [&](int id, int x, int y, int w, DWORD extra) -> HWND {
        HWND e = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER | extra,
            x, y, w, Sc(30), hwnd, (HMENU)(LONG_PTR)id, hi, NULL);
        SendMessageW(e, WM_SETFONT, (WPARAM)g_fonts.mono, TRUE);
        return e;
    };
    makeEdit(EDIT_SPEED,    Sc(368), Sc(ROW_SPEED_Y - 4),    Sc(EDIT_W), 0);
    makeEdit(EDIT_LOOPS,    Sc(368), Sc(ROW_LOOPS_Y - 4),    Sc(EDIT_W), ES_NUMBER);
    makeEdit(EDIT_INTERVAL, Sc(110), Sc(ROW_INTERVAL_Y - 4), Sc(70),     ES_NUMBER);

    // Toggle switches (owner-draw; label left + switch right, aligned with the edits)
    CreateFlowButton(hwnd, CHK_CONTINUOUS, cx, Sc(ROW_CONT_Y - 4), Sc(CTRL_W), Sc(30), L"Repeat playback until stopped");
    CreateFlowButton(hwnd, CHK_HUMANIZE,   cx, Sc(ROW_HUM_Y - 4),  Sc(CTRL_W), Sc(30), L"Add small random timing variance");

    // Footer: file ops grouped left; Settings + Stop All on the right
    int footY = Sc(FOOT_Y);
    CreateFlowButton(hwnd, BTN_OPEN, cx, footY, Sc(84), Sc(FOOT_H), L"Open a saved macro (.rec)");
    CreateFlowButton(hwnd, BTN_SAVE, cx + Sc(92), footY, Sc(84), Sc(FOOT_H), L"Save the current macro");
    int stopW = Sc(104), stopX = Sc(CRIGHT) - stopW;
    int setW = Sc(88), setX = stopX - Sc(10) - setW;
    CreateFlowButton(hwnd, BTN_SETTINGS, setX, footY, setW, Sc(FOOT_H), L"Hotkeys, always-on-top, about");
    CreateFlowButton(hwnd, BTN_STOP_ALL, stopX, footY, stopW, Sc(FOOT_H), L"Stop everything");

    // Initialize edit values from state
    wchar_t buf[32];
    swprintf(buf, 32, L"%g", g_app.playbackSpeed); SetDlgItemTextW(hwnd, EDIT_SPEED, buf);
    swprintf(buf, 32, L"%d", g_app.loopCount);      SetDlgItemTextW(hwnd, EDIT_LOOPS, buf);
    swprintf(buf, 32, L"%d", g_app.clickInterval);  SetDlgItemTextW(hwnd, EDIT_INTERVAL, buf);

    // Play is disabled until a macro exists
    EnableWindow(GetDlgItem(hwnd, BTN_PLAY),
                 g_app.engine && g_app.engine->GetEventCount() > 0);
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
    g_fonts.wordmark  = mkFont(Sc(28), FW_BOLD);
    g_fonts.cardTitle = mkFont(Sc(12), FW_BOLD);   // small tracked section labels
    g_fonts.button    = mkFont(Sc(17), FW_SEMIBOLD);
    g_fonts.body      = mkFont(Sc(16), FW_NORMAL);
    g_fonts.value     = mkFont(Sc(17), FW_SEMIBOLD);
    g_fonts.pill      = mkFont(Sc(15), FW_SEMIBOLD);
    g_fonts.small_    = mkFont(Sc(14), FW_NORMAL);
    g_fonts.mono      = CreateFontW(Sc(17), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

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
    DeleteObject(g_fonts.mono);
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
