/**
 * @file Hotkeys.h
 * @brief The four global hotkeys, and rendering a virtual-key code for display.
 *
 * They are system-wide (RegisterHotKey) rather than window-level, because the
 * point of a macro recorder is to be driven while another application has
 * focus. Re-register after any change so the new binding takes effect at once.
 */
#pragma once

#include <windows.h>

namespace flow::ui {

void RegisterHotkeys();
void UnregisterHotkeys();

/**
 * A printable name for a virtual-key code — "F8", "Ctrl+F9", "Key 190".
 * Returns a pointer to a static buffer, so copy it before the next call.
 */
const char* GetKeyName(UINT vk, bool withModifiers = false);

}  // namespace flow::ui
