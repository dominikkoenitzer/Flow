/**
 * @file Hotkeys.cpp
 * @brief Registration and naming for the global hotkeys.
 */
#include "Hotkeys.h"

#include "AppState.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace flow::ui {

bool RegisterHotkeys(std::string* taken) {
    const std::array<HotkeyBinding, 4> bindings = {{
        { HOTKEY_RECORD, 0, g_app.hotkeyRecord },
        { HOTKEY_PLAYBACK, g_app.hotkeyModifiers, g_app.hotkeyPlayback },
        { HOTKEY_CLICKER, 0, g_app.hotkeyClicker },
        { HOTKEY_STOP, 0, g_app.hotkeyStop },
    }};
    const int refused = RegisterBindings(bindings, [](const HotkeyBinding& b) {
        return RegisterHotKey(g_app.hwnd, b.id, b.modifiers, b.vk) != FALSE;
    });
    if (refused >= 0 && taken) {
        const HotkeyBinding& b = bindings[refused];
        *taken = GetKeyName(b.vk, b.id == HOTKEY_PLAYBACK);
    }

    // Keep the recorder's filter in step with the keys just registered.
    if (g_app.engine) {
        g_app.engine->SetControlKeys(g_app.hotkeyRecord, g_app.hotkeyPlayback,
                                     g_app.hotkeyClicker, g_app.hotkeyStop);
    }
    return refused < 0;
}

void ShowHotkeyTaken(HWND owner, const std::string& key) {
    const std::string text = "Another program already uses " + key + ", so FLOW cannot use it as a hotkey.";
    MessageBoxA(owner, text.c_str(), "Error", MB_OK | MB_ICONERROR);
}

void UnregisterHotkeys() {
    UnregisterHotKey(g_app.hwnd, HOTKEY_RECORD);
    UnregisterHotKey(g_app.hwnd, HOTKEY_PLAYBACK);
    UnregisterHotKey(g_app.hwnd, HOTKEY_CLICKER);
    UnregisterHotKey(g_app.hwnd, HOTKEY_STOP);
}

const char* GetKeyName(UINT vk, bool withModifiers) {
    static char keyName[64];
    keyName[0] = '\0';
    
    if (withModifiers && g_app.hotkeyModifiers) {
        if (g_app.hotkeyModifiers & MOD_CONTROL) strcat(keyName, "Ctrl+");
        if (g_app.hotkeyModifiers & MOD_SHIFT) strcat(keyName, "Shift+");
        if (g_app.hotkeyModifiers & MOD_ALT) strcat(keyName, "Alt+");
    }
    
    char key[32];
    switch (vk) {
        case VK_F1: strcpy(key, "F1"); break;
        case VK_F2: strcpy(key, "F2"); break;
        case VK_F3: strcpy(key, "F3"); break;
        case VK_F4: strcpy(key, "F4"); break;
        case VK_F5: strcpy(key, "F5"); break;
        case VK_F6: strcpy(key, "F6"); break;
        case VK_F7: strcpy(key, "F7"); break;
        case VK_F8: strcpy(key, "F8"); break;
        case VK_F9: strcpy(key, "F9"); break;
        case VK_F10: strcpy(key, "F10"); break;
        case VK_F11: strcpy(key, "F11"); break;
        case VK_F12: strcpy(key, "F12"); break;
        case VK_PAUSE: strcpy(key, "Pause"); break;
        default:
            if (vk >= 'A' && vk <= 'Z') {
                sprintf(key, "%c", vk);
            } else if (vk >= '0' && vk <= '9') {
                sprintf(key, "%c", vk);
            } else {
                sprintf(key, "Key %d", vk);
            }
            break;
    }
    strcat(keyName, key);
    return keyName;
}

}  // namespace flow::ui
