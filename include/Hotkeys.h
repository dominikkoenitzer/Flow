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

#include <array>
#include <string>

namespace flow::ui {

/** One global hotkey: its WM_HOTKEY id, the modifiers it needs and its key. */
struct HotkeyBinding {
    int id;
    UINT modifiers;
    UINT vk;
};

/** The keys the hotkey dialog accepts: F1 to F12, A to Z and 0 to 9. */
inline bool IsHotkeyKey(UINT vk) {
    return (vk >= VK_F1 && vk <= VK_F12) ||
           (vk >= 'A' && vk <= 'Z') ||
           (vk >= '0' && vk <= '9');
}

/**
 * The key a hotkey field takes from a keyboard message, or 0 when the message
 * does not set one. Only F1 to F12, A to Z and 0 to 9 are taken. F10 arrives
 * as WM_SYSKEYDOWN without Alt; a key pressed with Alt stays with the dialog.
 */
inline UINT HotkeyFieldKey(UINT msg, WPARAM wParam, LPARAM lParam) {
    const bool press = msg == WM_KEYDOWN ||
                       (msg == WM_SYSKEYDOWN && !(HIWORD(lParam) & KF_ALTDOWN));
    if (!press) return 0;
    const UINT vk = (UINT)wParam;
    return IsHotkeyKey(vk) ? vk : 0;
}

/**
 * Register every binding through `reg`, which the app points at RegisterHotKey,
 * and return the index of the first one refused, or -1 when none was.
 * A refused key does not stop the rest from being registered.
 */
template <typename Register>
int RegisterBindings(const std::array<HotkeyBinding, 4>& bindings, Register reg) {
    int refused = -1;
    for (int i = 0; i < (int)bindings.size(); ++i) {
        if (!reg(bindings[i]) && refused < 0) refused = i;
    }
    return refused;
}

/**
 * Register the four hotkeys. Returns false when Windows refused one, which it
 * does when another program already holds that key; `taken` then names it.
 */
bool RegisterHotkeys(std::string* taken = nullptr);
void UnregisterHotkeys();

/** Tell the user that another program holds `key`, so FLOW could not take it. */
void ShowHotkeyTaken(HWND owner, const std::string& key);

/**
 * A printable name for a virtual-key code: "F8", "Ctrl+F9", "Key 190".
 * Returns a pointer to a static buffer, so copy it before the next call.
 */
const char* GetKeyName(UINT vk, bool withModifiers = false);

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
