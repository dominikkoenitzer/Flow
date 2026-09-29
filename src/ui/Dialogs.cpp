/**
 * @file Dialogs.cpp
 * @brief Window classes, procedures and layout for the dialogs in Dialogs.h.
 */
#include "ui/Dialogs.h"

#include "AppState.h"
#include "Hotkeys.h"
#include "ui/Buttons.h"
#include "ui/Dpi.h"
#include "ui/Draw.h"
#include "ui/Theme.h"

#include <commctrl.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace flow::ui {


// Dialog controls
#define IDC_HOTKEY_RECORD 1007
#define IDC_HOTKEY_PLAYBACK 1008
#define IDC_HOTKEY_CLICKER 1009
#define IDC_HOTKEY_OK 1010
#define IDC_HOTKEY_CANCEL 1011

// Static IDs in the hotkey dialog, for layout and muted-text coloring
#define IDC_HK_SUBTITLE 9001
#define IDC_HK_INSTRUCTION 9002
#define IDC_HK_TITLE 9003
#define IDC_HK_LABEL_RECORD 9004
#define IDC_HK_LABEL_PLAYBACK 9005
#define IDC_HK_LABEL_CLICKER 9006
#define IDC_HK_LABEL_STOP 9007

// A dialog's own DPI and fonts, for the monitor the dialog is on, which need
// not be the main window's.
struct DialogScale {
    UINT dpi = 96;
    UiFonts fonts;
};

static DialogScale s_hotkeyScale;
static DialogScale s_aboutScale;

// Stands a dialog's scale and fonts in for g_scale and g_fonts while it lives,
// so Sc() and the shared painters size for the dialog's monitor. Never hold one
// across anything that pumps messages, or the main window would paint at the
// dialog's scale.
class ScaleScope {
public:
    explicit ScaleScope(const DialogScale& s) : scale_(g_scale), fonts_(g_fonts) {
        SetScaleFromDpi(s.dpi);
        g_fonts = s.fonts;
    }
    ~ScaleScope() {
        g_scale = scale_;
        g_fonts = fonts_;
    }
    ScaleScope(const ScaleScope&) = delete;
    ScaleScope& operator=(const ScaleScope&) = delete;

private:
    double scale_;
    UiFonts fonts_;
};

// Build a dialog's fonts for `dpi` and lay it out with them; the controls get
// the new fonts before the old ones are deleted.
static void ScaleDialog(HWND hDlg, DialogScale& s, UINT dpi, void (*layout)(HWND)) {
    UiFonts old = s.fonts;
    s.dpi = dpi;
    {
        ScaleScope scope(s);
        CreateUiFonts(s.fonts);
    }
    {
        ScaleScope scope(s);
        layout(hDlg);
    }
    DeleteUiFonts(old);
}

// Outer size of a dialog whose client area is w x h design units at `dpi`.
static SIZE DialogFrame(HWND hDlg, int w, int h, UINT dpi) {
    return WindowSizeForClient(ScAt(w, dpi), ScAt(h, dpi),
                               (DWORD)GetWindowLongPtrW(hDlg, GWL_STYLE),
                               (DWORD)GetWindowLongPtrW(hDlg, GWL_EXSTYLE), dpi);
}

// Where a dialog opens: centred on its parent, sized for the monitor it lands
// on. Plans at the parent's DPI, then again at the DPI of the monitor under
// that rectangle when it differs. Returns the rectangle and sets `dpi`.
static RECT PlanDialog(HWND hDlg, HWND parent, int w, int h, UINT& dpi) {
    RECT rp;
    GetWindowRect(parent, &rp);
    auto centred = [&](UINT d) {
        SIZE s = DialogFrame(hDlg, w, h, d);
        int x = rp.left + (rp.right - rp.left - s.cx) / 2;
        int y = rp.top + (rp.bottom - rp.top - s.cy) / 2;
        return RECT{ x, y, x + s.cx, y + s.cy };
    };
    dpi = DpiForWindow(parent);
    RECT r = centred(dpi);
    UINT monitorDpi = DpiForRect(r);
    if (monitorDpi != dpi) {
        dpi = monitorDpi;
        r = centred(dpi);
    }
    return r;
}

// The dialog manager's own DPI scaling only knows a template's controls and
// fonts, and would fight ours; FLOW's dialogs lay themselves out again instead.
static void DisableDialogAutoDpi(HWND hDlg) {
    typedef BOOL(WINAPI* SetDialogDpiChangeBehaviorFn)(HWND, int, int);
    static auto fn = (SetDialogDpiChangeBehaviorFn)(void*)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetDialogDpiChangeBehavior");
    if (fn) fn(hDlg, 1 /* DDC_DISABLE_ALL */, 1);
}

// A dialog dragged onto a monitor with another scale: rebuild its fonts and
// layout for the new DPI, take the rectangle Windows suggests, repaint.
// Returns true when `msg` was handled here.
static bool HandleDialogDpi(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam,
                            DialogScale& s, void (*layout)(HWND)) {
    if (msg != WM_DPICHANGED) return false;
    ScaleDialog(hDlg, s, LOWORD(wParam), layout);
    const RECT* r = (const RECT*)lParam;
    SetWindowPos(hDlg, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return true;
}

// Places a control at design units through Sc(), and optionally sets its font.
struct DlgPlace { int id, x, y, w, h; HFONT font; };

static void PlaceControls(HWND hDlg, const DlgPlace* places, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const DlgPlace& p = places[i];
        HWND c = GetDlgItem(hDlg, p.id);
        if (!c) continue;
        SetWindowPos(c, NULL, Sc(p.x), Sc(p.y), Sc(p.w), Sc(p.h),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (p.font) SendMessageW(c, WM_SETFONT, (WPARAM)p.font, TRUE);
    }
}

// Hotkey dialog layout, in design units (scaled via Sc).
static const int HK_W = 460, HK_H = 446, HK_PAD = 28;
static const int HK_ROW_TOP = 104, HK_ROW_H = 54, HK_EDIT_W = 150, HK_EDIT_H = 38;
static const int HK_EDIT_X = HK_W - HK_PAD - HK_EDIT_W;   // right-aligned key fields
static const int HK_BTN_Y = 384, HK_BTN_H = 44;

// Place every control in the hotkey dialog at the current g_scale and fonts.
static void LayoutHotkeyDialog(HWND hDlg) {
    const int textW = HK_W - 2 * HK_PAD;
    const int labelW = HK_EDIT_X - HK_PAD;
    const DlgPlace places[] = {
        { IDC_HK_TITLE,       HK_PAD, 20, textW, 36, g_fonts.wordmark },
        { IDC_HK_SUBTITLE,    HK_PAD, 62, textW, 24, g_fonts.small_ },
        { IDC_HK_LABEL_RECORD,   HK_PAD, HK_ROW_TOP + 10,                labelW, 24, g_fonts.body },
        { IDC_HK_LABEL_PLAYBACK, HK_PAD, HK_ROW_TOP + HK_ROW_H + 10,     labelW, 24, g_fonts.body },
        { IDC_HK_LABEL_CLICKER,  HK_PAD, HK_ROW_TOP + 2 * HK_ROW_H + 10, labelW, 24, g_fonts.body },
        { IDC_HK_LABEL_STOP,     HK_PAD, HK_ROW_TOP + 3 * HK_ROW_H + 10, labelW, 24, g_fonts.body },
        { IDC_HOTKEY_RECORD,   HK_EDIT_X, HK_ROW_TOP,                HK_EDIT_W, HK_EDIT_H, nullptr },
        { IDC_HOTKEY_PLAYBACK, HK_EDIT_X, HK_ROW_TOP + HK_ROW_H,     HK_EDIT_W, HK_EDIT_H, nullptr },
        { IDC_HOTKEY_CLICKER,  HK_EDIT_X, HK_ROW_TOP + 2 * HK_ROW_H, HK_EDIT_W, HK_EDIT_H, nullptr },
        { IDC_HOTKEY_STOP,     HK_EDIT_X, HK_ROW_TOP + 3 * HK_ROW_H, HK_EDIT_W, HK_EDIT_H, nullptr },
        { IDC_HK_INSTRUCTION, HK_PAD, HK_ROW_TOP + 4 * HK_ROW_H + 4, textW, 24, g_fonts.small_ },
        { IDC_HOTKEY_OK,     HK_W - HK_PAD - 270, HK_BTN_Y, 150, HK_BTN_H, nullptr },
        { IDC_HOTKEY_CANCEL, HK_W - HK_PAD - 110, HK_BTN_Y, 110, HK_BTN_H, nullptr },
    };
    PlaceControls(hDlg, places, sizeof(places) / sizeof(places[0]));
}

// Every dialog shares window class #32770, so the themed background is a
// class-wide patch: while one of ours is open, anything else on that class
// paints on BG_PRIMARY too, including the hotkey error boxes below, which keep
// the system text colours. Patches are counted so overlapping dialogs restore
// in any order and the last one out puts the original brush back.
static HBRUSH g_dlgPrevBrush = nullptr;
static int g_dlgBrushDepth = 0;

static void PatchDialogClassBrush(HWND hDlg) {
    HBRUSH prev = (HBRUSH)SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND,
                                           (LONG_PTR)CreateSolidBrush(BG_PRIMARY));
    if (g_dlgBrushDepth++ == 0) g_dlgPrevBrush = prev;
    else if (prev) DeleteObject(prev);
}

static void RestoreDialogClassBrush(HWND hDlg) {
    if (g_dlgBrushDepth == 0 || --g_dlgBrushDepth > 0) return;
    HBRUSH bg = (HBRUSH)SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND,
                                         (LONG_PTR)g_dlgPrevBrush);
    if (bg) DeleteObject(bg);
    g_dlgPrevBrush = nullptr;
}

// Hotkey dialog callback
LRESULT CALLBACK HotkeyDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (HandleDialogDpi(hDlg, msg, wParam, lParam, s_hotkeyScale, LayoutHotkeyDialog))
        return TRUE;
    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            static HBRUSH bgB = CreateSolidBrush(BG_PRIMARY);
            int id = GetDlgCtrlID((HWND)lParam);
            HDC dc = (HDC)wParam;
            bool muted = (id == IDC_HK_SUBTITLE || id == IDC_HK_INSTRUCTION);
            SetTextColor(dc, muted ? TEXT_SECONDARY : TEXT_PRIMARY);
            SetBkColor(dc, BG_PRIMARY);
            return (INT_PTR)bgB;
        }

        case WM_DRAWITEM: {
            ScaleScope scope(s_hotkeyScale);   // paint at the dialog's own DPI
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->CtlID == IDC_HOTKEY_OK)     { DrawDlgButton(dis, L"Save Changes", true); return TRUE; }
            if (dis->CtlID == IDC_HOTKEY_CANCEL) { DrawDlgButton(dis, L"Cancel", false);       return TRUE; }
            UINT vk = 0;
            if (dis->CtlID == IDC_HOTKEY_RECORD)        vk = g_tempHotkeyRecord;
            else if (dis->CtlID == IDC_HOTKEY_PLAYBACK) vk = g_tempHotkeyPlayback;
            else if (dis->CtlID == IDC_HOTKEY_CLICKER)  vk = g_tempHotkeyClicker;
            else if (dis->CtlID == IDC_HOTKEY_STOP)     vk = g_tempHotkeyStop;
            else break;
            wchar_t k[64];
            MultiByteToWideChar(CP_ACP, 0, GetKeyName(vk, false), -1, k, 64);
            DrawKeyField(dis, k);
            return TRUE;
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

        case WM_DESTROY: {
            RestoreDialogClassBrush(hDlg);
            g_hHotkeyRecordEdit = NULL;
            g_hHotkeyPlaybackEdit = NULL;
            g_hHotkeyClickerEdit = NULL;
            g_hHotkeyStopEdit = NULL;
            break;
        }
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
            if (hwnd == g_hHotkeyRecordEdit)        g_tempHotkeyRecord = vk;
            else if (hwnd == g_hHotkeyPlaybackEdit) g_tempHotkeyPlayback = vk;
            else if (hwnd == g_hHotkeyClickerEdit)  g_tempHotkeyClicker = vk;
            else if (hwnd == g_hHotkeyStopEdit)     g_tempHotkeyStop = vk;
            InvalidateRect(hwnd, NULL, FALSE);   // owner-draw repaints the key text
            return 0;
        }
    }
    // Repaint the focus ring as focus moves between fields.
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        InvalidateRect(hwnd, NULL, FALSE);
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
    PatchDialogClassBrush(hDlg);
    DisableDialogAutoDpi(hDlg);

    auto makeStatic = [&](int id, const wchar_t* text) {
        CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hDlg, (HMENU)(LONG_PTR)id, hi, NULL);
    };
    auto makeKeyField = [&](int id) -> HWND {
        HWND e = CreateWindowExW(0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hDlg, (HMENU)(LONG_PTR)id, hi, NULL);
        SetWindowSubclass(e, HotkeyEditProc, 0, 0);
        return e;
    };

    makeStatic(IDC_HK_TITLE, L"Customize Hotkeys");
    makeStatic(IDC_HK_SUBTITLE, L"Click a field, then press a key to rebind it.");

    makeStatic(IDC_HK_LABEL_RECORD,   L"Start / stop recording");
    g_hHotkeyRecordEdit   = makeKeyField(IDC_HOTKEY_RECORD);
    makeStatic(IDC_HK_LABEL_PLAYBACK, L"Start / stop playback");
    g_hHotkeyPlaybackEdit = makeKeyField(IDC_HOTKEY_PLAYBACK);
    makeStatic(IDC_HK_LABEL_CLICKER,  L"Toggle auto-clicker");
    g_hHotkeyClickerEdit  = makeKeyField(IDC_HOTKEY_CLICKER);
    makeStatic(IDC_HK_LABEL_STOP,     L"Stop all activities");
    g_hHotkeyStopEdit     = makeKeyField(IDC_HOTKEY_STOP);

    makeStatic(IDC_HK_INSTRUCTION,
        L"Supported keys: F1–F12, A–Z, 0–9.   Changes apply on Save.");

    CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hDlg, (HMENU)IDC_HOTKEY_OK, hi, NULL);
    CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, hDlg, (HMENU)IDC_HOTKEY_CANCEL, hi, NULL);

    // Centre on the parent, laid out for the monitor the dialog lands on.
    UINT dpi = 96;
    RECT at = PlanDialog(hDlg, hwnd, HK_W, HK_H, dpi);
    ScaleDialog(hDlg, s_hotkeyScale, dpi, LayoutHotkeyDialog);
    SetWindowPos(hDlg, HWND_TOP, at.left, at.top, at.right - at.left, at.bottom - at.top, 0);

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
    DeleteUiFonts(s_hotkeyScale.fonts);   // the dialog and its controls are gone
}

// ---------------------------------------------------------------------------
// About dialog
// ---------------------------------------------------------------------------

#define IDC_ABOUT_OK   9100
#define IDC_ABOUT_SUB  9101
#define IDC_ABOUT_BODY 9102
#define IDC_ABOUT_TITLE 9103

// About dialog layout, in design units (scaled via Sc).
static const int ABOUT_W = 440, ABOUT_H = 372, ABOUT_PAD = 28;

// Place every control in the About dialog at the current g_scale and fonts.
static void LayoutAboutDialog(HWND hDlg) {
    const int textW = ABOUT_W - 2 * ABOUT_PAD;
    const DlgPlace places[] = {
        { IDC_ABOUT_TITLE, ABOUT_PAD, 22,  textW, 34,  g_fonts.wordmark },
        { IDC_ABOUT_SUB,   ABOUT_PAD, 64,  textW, 22,  g_fonts.small_ },
        { IDC_ABOUT_BODY,  ABOUT_PAD, 100, textW, 180, g_fonts.body },
        { IDC_ABOUT_OK, ABOUT_W - ABOUT_PAD - 120, 300, 120, 44, nullptr },
    };
    PlaceControls(hDlg, places, sizeof(places) / sizeof(places[0]));
}

LRESULT CALLBACK AboutDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (HandleDialogDpi(hDlg, msg, wParam, lParam, s_aboutScale, LayoutAboutDialog))
        return TRUE;
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
            ScaleScope scope(s_aboutScale);   // paint at the dialog's own DPI
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
        case WM_DESTROY:
            RestoreDialogClassBrush(hDlg);
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
    PatchDialogClassBrush(hDlg);
    DisableDialogAutoDpi(hDlg);

    CreateWindowExW(0, L"STATIC", L"Flow", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, hDlg, (HMENU)IDC_ABOUT_TITLE, hi, NULL);

    CreateWindowExW(0, L"STATIC", L"Flexible Low-latency Operations Workflow",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0,
        hDlg, (HMENU)IDC_ABOUT_SUB, hi, NULL);

    CreateWindowExW(0, L"STATIC",
        L"A macro recorder, player, and high-speed auto-clicker.\r\n\r\n"
        L"Default hotkeys\r\n"
        L"F8:  Start / stop recording\r\n"
        L"F9:  Start / stop playback\r\n"
        L"F6:  Toggle auto-clicker\r\n"
        L"Pause:  Stop everything\r\n\r\n"
        L"Settings are saved between sessions.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0,
        hDlg, (HMENU)IDC_ABOUT_BODY, hi, NULL);

    CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hDlg, (HMENU)IDC_ABOUT_OK, hi, NULL);

    // Centre on the parent, laid out for the monitor the dialog lands on.
    UINT dpi = 96;
    RECT at = PlanDialog(hDlg, hwnd, ABOUT_W, ABOUT_H, dpi);
    ScaleDialog(hDlg, s_aboutScale, dpi, LayoutAboutDialog);
    SetWindowPos(hDlg, HWND_TOP, at.left, at.top, at.right - at.left, at.bottom - at.top, 0);

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
    DeleteUiFonts(s_aboutScale.fonts);   // the dialog and its controls are gone
}

}  // namespace flow::ui
