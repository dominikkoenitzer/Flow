/**
 * @file Dialogs.h
 * @brief FLOW's two modal dialogs.
 *
 * Both are ordinary registered window classes rather than resource templates,
 * so they can use the same owner-draw buttons, fonts and palette as the main
 * window instead of the system dialog look.
 */
#pragma once

#include <windows.h>

namespace flow::ui {

/**
 * Rebind the four global hotkeys. Edits scratch copies and only commits them to
 * the app state — and re-registers — when the user saves.
 */
void ShowCustomizeHotkeysDialog(HWND hwnd);

/** Version, author and licence. */
void ShowAboutDialog(HWND hwnd);

}  // namespace flow::ui
