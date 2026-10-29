/**
 * @file test_hotkeys.cpp
 * @brief Which key presses set a hotkey.
 */
#include "doctest.h"

#include "Hotkeys.h"

using flow::ui::HotkeyFieldKey;

namespace {

/** lParam of a key pressed while Alt is held. */
const LPARAM kAltDown = (LPARAM)KF_ALTDOWN << 16;

}  // namespace

TEST_CASE("F10 can be set as a hotkey, though it arrives as a system key") {
    CHECK(HotkeyFieldKey(WM_SYSKEYDOWN, VK_F10, 0) == VK_F10);
}

TEST_CASE("Plain key presses set a hotkey") {
    CHECK(HotkeyFieldKey(WM_KEYDOWN, VK_F1, 0) == VK_F1);
    CHECK(HotkeyFieldKey(WM_KEYDOWN, VK_F12, 0) == VK_F12);
    CHECK(HotkeyFieldKey(WM_KEYDOWN, 'A', 0) == 'A');
    CHECK(HotkeyFieldKey(WM_KEYDOWN, '9', 0) == '9');
}

TEST_CASE("Keys pressed with Alt are left to the dialog") {
    CHECK(HotkeyFieldKey(WM_SYSKEYDOWN, VK_F4, kAltDown) == 0);
    CHECK(HotkeyFieldKey(WM_SYSKEYDOWN, VK_F10, kAltDown) == 0);
    CHECK(HotkeyFieldKey(WM_SYSKEYDOWN, 'S', kAltDown) == 0);
}

TEST_CASE("Releases and keys outside the list do not set a hotkey") {
    CHECK(HotkeyFieldKey(WM_KEYUP, VK_F8, 0) == 0);
    CHECK(HotkeyFieldKey(WM_SYSKEYUP, VK_F10, 0) == 0);
    CHECK(HotkeyFieldKey(WM_KEYDOWN, VK_SPACE, 0) == 0);
    CHECK(HotkeyFieldKey(WM_KEYDOWN, VK_PAUSE, 0) == 0);
}
