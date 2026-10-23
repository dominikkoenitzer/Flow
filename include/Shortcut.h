/**
 * @file Shortcut.h
 * @brief A Windows shortcut that plays one macro: `FLOW.exe --play "x.rec"`.
 */
#pragma once

#include <windows.h>
#include <string>

namespace flow::ui {

/**
 * Ask where to save a shortcut that plays `macro` with this FLOW.exe, offering
 * the desktop and the macro's name, and write it there. Says so when it fails.
 */
void CreatePlayShortcut(HWND owner, const std::wstring& macro);

}  // namespace flow::ui
