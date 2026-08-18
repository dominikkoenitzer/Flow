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

}  // namespace flow::ui
