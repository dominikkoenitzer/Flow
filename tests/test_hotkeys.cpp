/**
 * @file test_hotkeys.cpp
 * @brief IsHotkeyKey, the keys the hotkey dialog accepts, and SettleHotkeys,
 * which applies the same rule to the four hotkeys read from the settings file.
 *
 * The keys are in the order record, playback, clicker, stop, with the
 * defaults from AppState.h.
 */
#include "doctest.h"

#include "Hotkeys.h"

using flow::ui::IsHotkeyKey;
using flow::ui::SettleHotkeys;

namespace {

constexpr UINT kDefaults[4] = {VK_F8, VK_F9, VK_F6, VK_PAUSE};

void check_keys(const UINT (&keys)[4], UINT record, UINT playback, UINT clicker, UINT stop) {
    CHECK(keys[0] == record);
    CHECK(keys[1] == playback);
    CHECK(keys[2] == clicker);
    CHECK(keys[3] == stop);
}

}  // namespace

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
