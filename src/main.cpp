/**
 * @file main.cpp
 * @brief FLOW - Modern Professional UI Design
 * @author FLOW Development Team
 * @version 3.0.0
 */

#include "FlowEngine.h"
#include "AppState.h"
#include "Settings.h"
#include "Hotkeys.h"
#include "ui/Buttons.h"
#include "ui/Dialogs.h"
#include "ui/Draw.h"
#include "ui/Theme.h"

#include <windows.h>
#include <commctrl.h>
#include <shellscalingapi.h>
#include <shellapi.h>   // drag-and-drop (DragAcceptFiles / DragQueryFile)
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
using namespace flow::ui;

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
    AppendMenuW(hMenu, MF_STRING | (g_app.reopenLastMacro ? MF_CHECKED : 0),
                MENU_REOPEN_LAST, L"Reopen Last Macro on Launch");
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
            g_app.lastMacroPath = szFile;
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
        } else {
            g_app.lastMacroPath = szFile;
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
        // StartRecording installs the global hooks on demand; if that fails it
        // stays inactive. Mirror the engine's real state instead of optimistically
        // claiming "Recording", so the UI can't lie about what's happening.
        g_app.isRecording = g_app.engine->IsRecordingActive();
        if (g_app.isRecording) {
            SetWindowTextA(hwnd, "FLOW - Recording...");
        } else {
            MessageBoxW(hwnd,
                L"Couldn't start recording — input hooks failed to install.\n\n"
                L"Make sure FLOW is running as Administrator.",
                L"Recording", MB_OK | MB_ICONWARNING);
        }
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

// ============================================================================
// DIALOG BOXES
// ============================================================================
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

// Hover tracking for the borderless number fields (ghost-stepper affordance).
static HWND g_hoverEdit = nullptr;
LRESULT CALLBACK InputEditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    (void)id; (void)ref;
    if (m == WM_MOUSEMOVE && g_hoverEdit != h) {
        g_hoverEdit = h;
        TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, h, 0 };
        TrackMouseEvent(&t);
        InvalidateRect(GetParent(h), NULL, FALSE);
    } else if (m == WM_MOUSELEAVE) {
        g_hoverEdit = nullptr;
        InvalidateRect(GetParent(h), NULL, FALSE);
    }
    return DefSubclassProc(h, m, w, l);
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

    // ---- status state (idle = soft slate dot; real color only when active) ----
    const wchar_t* stTxt; COLORREF stDot, stTextCol;
    if (g_app.isRecording)     { stTxt = L"Recording"; stDot = DANGER_COLOR;   stTextCol = DANGER_COLOR; }
    else if (g_app.isPlaying)  { stTxt = L"Playing";   stDot = SUCCESS_COLOR;  stTextCol = SUCCESS_COLOR; }
    else if (g_app.isClicking) { stTxt = L"Clicking";  stDot = ACCENT_PRIMARY; stTextCol = ACCENT_PRIMARY; }
    else                       { stTxt = L"Ready";     stDot = STATUS_IDLE;    stTextCol = TEXT_SECONDARY; }

    HFONT of = (HFONT)SelectObject(hdc, g_fonts.pill);
    SIZE sts; GetTextExtentPoint32W(hdc, stTxt, (int)wcslen(stTxt), &sts);
    SelectObject(hdc, of);
    int dotR = Sc(5), dotGap = Sc(9);
    int stX = cright - (dotR * 2 + dotGap + sts.cx);
    int stCy = Sc(WORDMARK_Y) + Sc(10);

    // ---- dividers + status dot + input pills ----
    HWND focus = GetFocus();
    struct E { int id, x, y, w; };
    E edits[] = {
        { EDIT_SPEED,    CTRL_RIGHT - EDIT_W, ROW_SPEED_Y - 4,    EDIT_W },
        { EDIT_LOOPS,    CTRL_RIGHT - EDIT_W, ROW_LOOPS_Y - 4,    EDIT_W },
        { EDIT_INTERVAL, CTRL_RIGHT - 70,     ROW_INTERVAL_Y - 4, 70 },
    };
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen div(GP(BORDER_DEFAULT), Scf(1.0f));
        int ys[] = { Sc(REC_DIV_Y), Sc(PLAY_DIV_Y), Sc(CLK_DIV_Y), Sc(FOOT_DIV_Y) };
        for (int yy : ys) g.DrawLine(&div, (float)cx, (float)yy, (float)cright, (float)yy);
        IconCircle(g, (float)(stX + dotR), (float)stCy, (float)dotR, GP(stDot));

        // Ghost-stepper fields: quiet hairline at rest, lift on hover, accent ring on focus.
        for (const E& e : edits) {
            HWND eh = g_app.hwnd ? GetDlgItem(g_app.hwnd, e.id) : nullptr;
            bool foc = (eh && eh == focus), hov = (eh && eh == g_hoverEdit);
            float px = Sc(e.x) - Scf(5), py = Sc(e.y) - Scf(3);
            float pw = Sc(e.w) + Scf(10), ph = Sc(30) + Scf(6);
            FillRound(g, px, py, pw, ph, Scf(8.0f), GP((foc || hov) ? TRACK_HOVER : TRACK_BG));
            COLORREF bc = foc ? ACCENT_PRIMARY : BORDER_DEFAULT;
            StrokeRound(g, px, py, pw, ph, Scf(8.0f), GP(bc), foc ? Scf(1.6f) : Scf(1.0f));
        }
    }
    {
        HFONT o = (HFONT)SelectObject(hdc, g_fonts.pill);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, stTextCol);
        RECT tr = { stX + dotR * 2 + dotGap, stCy - Sc(14), cright, stCy + Sc(14) };
        DrawTextW(hdc, stTxt, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hdc, o);
    }

    // ---- header (small brand wordmark above the descriptor) ----
    TextLine(hdc, L"Flow", cx, Sc(WORDMARK_Y), g_fonts.wordmark, TEXT_PRIMARY);
    TextLine(hdc, L"Macro recorder & auto-clicker", cx, Sc(SUBTITLE_Y), g_fonts.small_, TEXT_SECONDARY);

    // ---- section labels (neutral grey, not the accent blue) ----
    TextLine(hdc, L"RECORD",       cx, Sc(REC_LABEL_Y),  g_fonts.cardTitle, LABEL_GREY);
    TextLine(hdc, L"PLAYBACK",     cx, Sc(PLAY_LABEL_Y), g_fonts.cardTitle, LABEL_GREY);
    TextLine(hdc, L"AUTO-CLICKER", cx, Sc(CLK_LABEL_Y),  g_fonts.cardTitle, LABEL_GREY);

    // ---- record info / empty state (single readable line) ----
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
            TextLine(hdc, cap, cx, Sc(REC_INFO_Y), g_fonts.mono, TEXT_SECONDARY);
        } else {
            wchar_t hint[96], wkn[32];
            MultiByteToWideChar(CP_ACP, 0, GetKeyName(g_app.hotkeyRecord, false), -1, wkn, 32);
            swprintf(hint, 96, L"Press %ls to record, or open a saved .rec file.", wkn);
            TextLine(hdc, hint, cx, Sc(REC_INFO_Y), g_fonts.body, TEXT_SECONDARY);
        }
    }

    // ---- playback row labels (controls are right-aligned child widgets) ----
    TextLine(hdc, L"Speed", cx, Sc(ROW_SPEED_Y), g_fonts.body, TEXT_SECONDARY);
    TextLine(hdc, L"×", Sc(CTRL_RIGHT) + Sc(4), Sc(ROW_SPEED_Y), g_fonts.body, TEXT_FAINT);
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

    // ---- auto-clicker (set apart by its own caption — works without a macro) ----
    TextLine(hdc, L"Click repeatedly at a set interval.",
             cx, Sc(CLK_CAPTION_Y), g_fonts.small_, TEXT_FAINT);
    TextLine(hdc, L"Interval (ms)", cx, Sc(ROW_INTERVAL_Y), g_fonts.body, TEXT_SECONDARY);
    {
        wchar_t cps[48];
        int per = g_app.clickInterval > 0 ? g_app.clickInterval : 1;
        swprintf(cps, 48, L"≈ %d clicks/sec", (1000 + per / 2) / per);
        RECT hr = { cx, Sc(INTERVAL_HELP_Y), Sc(CTRL_RIGHT), Sc(INTERVAL_HELP_Y) + Sc(20) };
        HFONT o = (HFONT)SelectObject(hdc, g_fonts.small_);
        SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, TEXT_FAINT);
        DrawTextW(hdc, cps, -1, &hr, DT_RIGHT | DT_SINGLELINE | DT_TOP);
        SelectObject(hdc, o);
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
                case MENU_REOPEN_LAST:
                    g_app.reopenLastMacro = !g_app.reopenLastMacro;
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

        case WM_DROPFILES: {
            // Load a .rec dropped onto the window.
            HDROP drop = (HDROP)wParam;
            wchar_t path[MAX_PATH] = {0};
            if (DragQueryFileW(drop, 0, path, MAX_PATH) && g_app.engine) {
                if (g_app.engine->LoadMacro(path)) {
                    g_app.lastMacroPath = path;
                    UpdateStatusDisplay();
                } else {
                    MessageBoxW(hwnd, L"That file isn't a valid FLOW macro (.rec).",
                                L"Open", MB_OK | MB_ICONWARNING);
                }
            }
            DragFinish(drop);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY: {
            // Remember the window position (ignore the -32000 minimized sentinel).
            RECT wr; GetWindowRect(hwnd, &wr);
            if (wr.left > -30000 && wr.top > -30000) {
                g_app.winX = wr.left; g_app.winY = wr.top; g_app.hasWinPos = true;
            }
            SaveSettings();
            KillTimer(hwnd, TIMER_STATUS_CHECK);
            PostQuitMessage(0);
            break;
        }

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

    // Inline numeric edits — borderless, monospace, right-aligned; right edge at CTRL_RIGHT.
    // Pill + hover/focus affordance is painted in PaintUI; hover tracked via InputEditProc.
    auto makeEdit = [&](int id, int x, int y, int w, DWORD extra) -> HWND {
        HWND e = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_RIGHT | extra,
            x, y, w, Sc(30), hwnd, (HMENU)(LONG_PTR)id, hi, NULL);
        SendMessageW(e, WM_SETFONT, (WPARAM)g_fonts.mono, TRUE);
        SetWindowSubclass(e, InputEditProc, 0, 0);
        return e;
    };
    makeEdit(EDIT_SPEED,    Sc(CTRL_RIGHT - EDIT_W), Sc(ROW_SPEED_Y - 4),    Sc(EDIT_W), 0);
    makeEdit(EDIT_LOOPS,    Sc(CTRL_RIGHT - EDIT_W), Sc(ROW_LOOPS_Y - 4),    Sc(EDIT_W), ES_NUMBER);
    makeEdit(EDIT_INTERVAL, Sc(CTRL_RIGHT - 70),     Sc(ROW_INTERVAL_Y - 4), Sc(70),     ES_NUMBER);

    // Toggle switches (owner-draw; label left + switch right, aligned with the edits)
    CreateFlowButton(hwnd, CHK_CONTINUOUS, cx, Sc(ROW_CONT_Y - 4), Sc(CTRL_W), Sc(30), L"Repeat playback until stopped");
    CreateFlowButton(hwnd, CHK_HUMANIZE,   cx, Sc(ROW_HUM_Y - 4),  Sc(CTRL_W), Sc(30), L"Add small random timing variance");

    // Footer: Open+Save grouped left, big gap, Settings+Stop All grouped right
    int footY = Sc(FOOT_Y);
    CreateFlowButton(hwnd, BTN_OPEN, cx, footY, Sc(80), Sc(FOOT_H), L"Open a saved macro (.rec)");
    CreateFlowButton(hwnd, BTN_SAVE, cx + Sc(88), footY, Sc(80), Sc(FOOT_H), L"Save the current macro");
    int stopW = Sc(92), stopX = Sc(CRIGHT) - stopW;
    int setW = Sc(84), setX = stopX - Sc(8) - setW;
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

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok) {
        // The entire UI is GDI+-rendered; without it the window can't draw, so
        // fail loudly instead of showing a broken/blank window.
        MessageBoxW(NULL, L"Failed to initialize graphics (GDI+).", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

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
    g_fonts.wordmark  = mkFont(Sc(22), FW_BOLD);
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

    // Load persisted settings up front so the saved window position is available
    // before the window is created.
    LoadSettings();

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

    // Restore the saved window position, clamped to the visible desktop so it can
    // never be stranded off-screen; otherwise center on the primary monitor.
    int posX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
    if (g_app.hasWinPos) {
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        posX = g_app.winX; posY = g_app.winY;
        if (posX < vx) posX = vx;
        if (posY < vy) posY = vy;
        if (posX > vx + vw - winW) posX = vx + vw - winW;
        if (posY > vy + vh - winH) posY = vy + vh - winH;
    }

    g_app.hwnd = CreateWindowExW(0, L"FLOW_Modern", L"FLOW",
        style, posX, posY, winW, winH, NULL, NULL, hInstance, NULL);

    if (!g_app.hwnd) {
        MessageBoxW(NULL, L"Window creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    DragAcceptFiles(g_app.hwnd, TRUE);  // accept .rec files dropped onto the window

    g_app.engine = new FlowEngine();

    // Apply persisted settings (loaded above) to the engine.
    g_app.engine->SetPlaybackSpeed(g_app.playbackSpeed);
    g_app.engine->EnableHumanization(g_app.humanizationEnabled);
    g_app.engine->ConfigureHumanization(0.0, g_app.humanizationStdDev);

    // Optionally reopen the last macro so playback is ready immediately on launch.
    if (g_app.reopenLastMacro && !g_app.lastMacroPath.empty()) {
        g_app.engine->LoadMacro(g_app.lastMacroPath);  // silently ignored if missing
    }

    // Probe input-hook access at startup so we can fail fast with a clear message
    // if not elevated. The hooks are then released immediately and re-installed
    // on demand only while recording (see StartRecording), so FLOW adds no
    // overhead to system-wide input while idle, playing back, or auto-clicking.
    if (!g_app.engine->InstallHooks()) {
        MessageBoxW(NULL, L"Failed to install input hooks!\n\nRun as Administrator.",
            L"Error", MB_OK | MB_ICONERROR);
        delete g_app.engine;
        return 1;
    }
    g_app.engine->UninstallHooks();

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
