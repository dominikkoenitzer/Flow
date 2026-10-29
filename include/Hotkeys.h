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
 * A printable name for a virtual-key code: "F8", "Ctrl+F9", "Key 190".
 * Returns a pointer to a static buffer, so copy it before the next call.
 */
const char* GetKeyName(UINT vk, bool withModifiers = false);

/** The keys the hotkey dialog accepts: F1 to F12, A to Z and 0 to 9. */
inline bool IsHotkeyKey(UINT vk) {
    return (vk >= VK_F1 && vk <= VK_F12) ||
           (vk >= 'A' && vk <= 'Z') ||
           (vk >= '0' && vk <= '9');
}

/** Holds loaded hotkeys (record, playback, clicker, stop) to the dialog's rule: a key it refuses, or two actions on one key, fall back to the defaults. */
inline void SettleHotkeys(UINT (&keys)[4], const UINT (&defaults)[4]) {
    for (int i = 0; i < 4; ++i) {
        if (!IsHotkeyKey(keys[i])) keys[i] = defaults[i];
    }
    // Each pass moves at least one key to its default, so this ends.
    for (bool clash = true; clash;) {
        clash = false;
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                if (keys[i] != keys[j]) continue;
                if (keys[i] == defaults[i] && keys[j] == defaults[j]) continue;
                keys[i] = defaults[i];
                keys[j] = defaults[j];
                clash = true;
            }
        }
    }
}

}  // namespace flow::ui
