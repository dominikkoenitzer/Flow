/**
 * @file AppState.cpp
 * @brief Definitions for the globals declared in AppState.h and Theme.h.
 */
#include "AppState.h"
#include "ui/Theme.h"

namespace flow::ui {

double g_scale = 1.0;

UiFonts g_fonts;
AppState g_app;

UINT g_tempHotkeyRecord   = VK_F8;
UINT g_tempHotkeyPlayback = VK_F9;
UINT g_tempHotkeyClicker  = VK_F6;
UINT g_tempHotkeyStop     = VK_PAUSE;

HWND g_hHotkeyRecordEdit   = nullptr;
HWND g_hHotkeyPlaybackEdit = nullptr;
HWND g_hHotkeyClickerEdit  = nullptr;
HWND g_hHotkeyStopEdit     = nullptr;

static HFONT MakeFont(int h, int weight) {
    return CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void CreateUiFonts(UiFonts& f) {
    f.wordmark  = MakeFont(Sc(22), FW_BOLD);
    f.cardTitle = MakeFont(Sc(12), FW_BOLD);   // small tracked section labels
    f.button    = MakeFont(Sc(17), FW_SEMIBOLD);
    f.body      = MakeFont(Sc(16), FW_NORMAL);
    f.value     = MakeFont(Sc(17), FW_SEMIBOLD);
    f.pill      = MakeFont(Sc(15), FW_SEMIBOLD);
    f.small_    = MakeFont(Sc(14), FW_NORMAL);
    f.mono      = CreateFontW(Sc(17), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
}

void DeleteUiFonts(UiFonts& f) {
    HFONT* all[] = { &f.wordmark, &f.cardTitle, &f.button, &f.body,
                     &f.value, &f.pill, &f.small_, &f.mono };
    for (HFONT* h : all) {
        if (*h) DeleteObject(*h);
        *h = nullptr;
    }
}

}  // namespace flow::ui
