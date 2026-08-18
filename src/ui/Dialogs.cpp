/**
 * @file Dialogs.cpp
 * @brief Window classes, procedures and layout for the dialogs in Dialogs.h.
 */
#include "ui/Dialogs.h"

#include "AppState.h"
#include "Hotkeys.h"
#include "ui/Buttons.h"
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

// Static IDs used only for muted-text coloring in the hotkey dialog
#define IDC_HK_SUBTITLE 9001
#define IDC_HK_INSTRUCTION 9002

// Hotkey dialog callback
LRESULT CALLBACK HotkeyDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
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
    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(BG_PRIMARY));

    // Layout in design units (scaled via Sc).
    const int W = 460, H = 446, padX = 28;
    const int rowTop = 104, rowH = 54, editW = 150, editH = 38;
    const int editX = W - padX - editW;   // right-aligned key fields

    HWND title = CreateWindowExW(0, L"STATIC", L"Customize Hotkeys",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(20), Sc(W - 2 * padX), Sc(36),
        hDlg, NULL, hi, NULL);
    SendMessageW(title, WM_SETFONT, (WPARAM)g_fonts.wordmark, TRUE);

    HWND sub = CreateWindowExW(0, L"STATIC",
        L"Click a field, then press a key to rebind it.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(62), Sc(W - 2 * padX), Sc(24),
        hDlg, (HMENU)IDC_HK_SUBTITLE, hi, NULL);
    SendMessageW(sub, WM_SETFONT, (WPARAM)g_fonts.small_, TRUE);

    auto makeRow = [&](const wchar_t* text, int editId, int y) -> HWND {
        HWND lab = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
            Sc(padX), Sc(y + 10), Sc(editX - padX), Sc(24), hDlg, NULL, hi, NULL);
        SendMessageW(lab, WM_SETFONT, (WPARAM)g_fonts.body, TRUE);
        HWND e = CreateWindowExW(0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            Sc(editX), Sc(y), Sc(editW), Sc(editH), hDlg, (HMENU)(LONG_PTR)editId, hi, NULL);
        SetWindowSubclass(e, HotkeyEditProc, 0, 0);
        return e;
    };

    g_hHotkeyRecordEdit   = makeRow(L"Start / stop recording", IDC_HOTKEY_RECORD,   rowTop);
    g_hHotkeyPlaybackEdit = makeRow(L"Start / stop playback",  IDC_HOTKEY_PLAYBACK, rowTop + rowH);
    g_hHotkeyClickerEdit  = makeRow(L"Toggle auto-clicker",    IDC_HOTKEY_CLICKER,  rowTop + 2 * rowH);
    g_hHotkeyStopEdit     = makeRow(L"Stop all activities",    IDC_HOTKEY_STOP,     rowTop + 3 * rowH);

    HWND instr = CreateWindowExW(0, L"STATIC",
        L"Supported keys: F1–F12, A–Z, 0–9.   Changes apply on Save.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, Sc(padX), Sc(rowTop + 4 * rowH + 4), Sc(W - 2 * padX), Sc(24),
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

// ---------------------------------------------------------------------------
// About dialog
// ---------------------------------------------------------------------------

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

    HWND title = CreateWindowExW(0, L"STATIC", L"Flow", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Sc(padX), Sc(22), Sc(W - 2 * padX), Sc(34), hDlg, NULL, hi, NULL);
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

}  // namespace flow::ui
