/**
 * @file test_hotkeys.cpp
 * @brief Which key presses set a hotkey, how a refused registration is
 * reported, and SettleHotkeys, which holds the four hotkeys read from the
 * settings file to the keys the dialog accepts.
 *
 * Registration goes through a stand-in, so no test registers a real global hotkey.
 * Loaded keys are in the order record, playback, clicker, stop, with the
 * defaults from AppState.h.
 */
#include "doctest.h"

#include "Hotkeys.h"

#include <vector>

using flow::ui::HotkeyBinding;
using flow::ui::HotkeyFieldKey;
using flow::ui::IsHotkeyKey;
using flow::ui::RegisterBindings;
using flow::ui::SettleHotkeys;

namespace {

const std::array<HotkeyBinding, 4> kBindings = {{
    { 301, 0, VK_F8 },
    { 302, MOD_CONTROL, VK_F9 },
    { 303, 0, VK_F6 },
    { 304, 0, VK_F10 },
}};

/** lParam of a key pressed while Alt is held. */
const LPARAM kAltDown = (LPARAM)KF_ALTDOWN << 16;

constexpr UINT kDefaults[4] = {VK_F8, VK_F9, VK_F6, VK_PAUSE};

void check_keys(const UINT (&keys)[4], UINT record, UINT playback, UINT clicker, UINT stop) {
    CHECK(keys[0] == record);
    CHECK(keys[1] == playback);
    CHECK(keys[2] == clicker);
    CHECK(keys[3] == stop);
}

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

TEST_CASE("A key another program holds is reported as refused") {
    std::vector<UINT> tried;
    const int refused = RegisterBindings(kBindings, [&](const HotkeyBinding& b) {
        tried.push_back(b.vk);
        return b.vk != VK_F9;
    });
    CHECK(refused == 1);
    CHECK(tried == std::vector<UINT>{ VK_F8, VK_F9, VK_F6, VK_F10 });
}

TEST_CASE("The first refused key is the one reported, and the rest are still tried") {
    std::vector<UINT> tried;
    const int refused = RegisterBindings(kBindings, [&](const HotkeyBinding& b) {
        tried.push_back(b.vk);
        return b.vk != VK_F8 && b.vk != VK_F10;
    });
    CHECK(refused == 0);
    CHECK(tried.size() == 4);
}

TEST_CASE("Each binding is registered with its own id and modifiers") {
    std::vector<HotkeyBinding> seen;
    const int refused = RegisterBindings(kBindings, [&](const HotkeyBinding& b) {
        seen.push_back(b);
        return true;
    });
    CHECK(refused == -1);
    REQUIRE(seen.size() == 4);
    CHECK(seen[1].id == 302);
    CHECK(seen[1].modifiers == MOD_CONTROL);
    CHECK(seen[0].modifiers == 0);
}

TEST_CASE("The dialog's keys are F1 to F12, A to Z and 0 to 9") {
    CHECK(IsHotkeyKey(VK_F1));
    CHECK(IsHotkeyKey(VK_F12));
    CHECK(IsHotkeyKey('A'));
    CHECK(IsHotkeyKey('Z'));
    CHECK(IsHotkeyKey('0'));
    CHECK(IsHotkeyKey('9'));

    CHECK_FALSE(IsHotkeyKey(VK_F1 - 1));
    CHECK_FALSE(IsHotkeyKey(VK_F13));
    CHECK_FALSE(IsHotkeyKey('A' - 1));
    CHECK_FALSE(IsHotkeyKey('Z' + 1));
    CHECK_FALSE(IsHotkeyKey('0' - 1));
    CHECK_FALSE(IsHotkeyKey('9' + 1));
    CHECK_FALSE(IsHotkeyKey(0));
    CHECK_FALSE(IsHotkeyKey(3));
    CHECK_FALSE(IsHotkeyKey(70000));
    CHECK_FALSE(IsHotkeyKey(VK_PAUSE));
}

TEST_CASE("Loaded keys the dialog accepts are kept") {
    UINT keys[4] = {VK_F1, 'Z', '0', VK_F12};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F1, 'Z', '0', VK_F12);
}

TEST_CASE("The defaults are kept, Pause for stop included") {
    UINT keys[4] = {VK_F8, VK_F9, VK_F6, VK_PAUSE};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, VK_F6, VK_PAUSE);
}

TEST_CASE("A loaded key outside the dialog's set falls back to that action's default") {
    UINT keys[4] = {3, 70000, VK_LWIN, (UINT)-5};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, VK_F6, VK_PAUSE);
}

TEST_CASE("Pause is kept only on the action whose default it is") {
    UINT keys[4] = {VK_PAUSE, VK_F9, VK_F6, VK_F12};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, VK_F6, VK_F12);
}

TEST_CASE("Two actions on one key both fall back to their defaults") {
    UINT keys[4] = {'A', 'A', 'B', 'C'};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, 'B', 'C');
}

TEST_CASE("A loaded key on another action's default sends both back to theirs") {
    UINT keys[4] = {VK_F9, VK_F9, VK_F6, VK_PAUSE};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, VK_F6, VK_PAUSE);
}

TEST_CASE("A fallback that makes a new clash is settled too") {
    UINT keys[4] = {VK_F9, VK_F8, VK_F9, VK_PAUSE};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F8, VK_F9, VK_F6, VK_PAUSE);
}

TEST_CASE("Swapped keys are not a clash") {
    UINT keys[4] = {VK_F9, VK_F8, VK_F6, VK_PAUSE};
    SettleHotkeys(keys, kDefaults);
    check_keys(keys, VK_F9, VK_F8, VK_F6, VK_PAUSE);
}
