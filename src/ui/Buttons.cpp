/**
 * @file Buttons.cpp
 * @brief Painting for the owner-draw controls declared in Buttons.h.
 */
#include "ui/Buttons.h"

#include "AppState.h"
#include "Hotkeys.h"
#include "ui/Draw.h"
#include "ui/Theme.h"

#include <commctrl.h>
#include <gdiplus.h>

namespace flow::ui {


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
void DrawFlowButton(DRAWITEMSTRUCT* dis) {
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
void DrawToggle(DRAWITEMSTRUCT* dis, const wchar_t* label, bool on) {
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

// A read-only key field rendered as a soft input pill (matches the main window
// number fields): hairline at rest, lifted fill + accent ring when focused.
void DrawKeyField(DRAWITEMSTRUCT* dis, const wchar_t* keyText) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    bool foc = (dis->itemState & ODS_FOCUS) != 0;

    HBRUSH bb = CreateSolidBrush(BG_PRIMARY);
    FillRect(hdc, &rc, bb);
    DeleteObject(bb);
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        FillRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, Scf(8.0f), GP(foc ? TRACK_HOVER : TRACK_BG));
        StrokeRound(g, 0.5f, 0.5f, (float)w - 1, (float)h - 1, Scf(8.0f),
                    GP(foc ? ACCENT_PRIMARY : BORDER_DEFAULT), foc ? Scf(1.6f) : Scf(1.0f));
    }
    HFONT old = (HFONT)SelectObject(hdc, g_fonts.mono);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_PRIMARY);
    DrawTextW(hdc, keyText, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(hdc, old);
}

// A centered, rounded dialog button (primary = filled accent, else ghost).
// Dialogs sit on a BG_PRIMARY surface.
void DrawDlgButton(DRAWITEMSTRUCT* dis, const wchar_t* label, bool primary) {
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

}  // namespace flow::ui
