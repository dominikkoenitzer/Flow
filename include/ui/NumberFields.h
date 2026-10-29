/**
 * @file NumberFields.h
 * @brief The main window's number fields, and when a hotkey takes their value.
 *
 * A number field's value is read when the field loses focus. A global hotkey
 * leaves the focus where it was, so it takes the focused field's value before
 * it acts. No Windows calls, so the tests can check it without a window.
 */
#pragma once

#include "AppState.h"

namespace flow::ui {

/** Whether `id` is Speed, Loops, Interval, Jitter or the click limit. */
inline bool IsNumberField(int id) {
    return id == EDIT_SPEED || id == EDIT_LOOPS || id == EDIT_INTERVAL
        || id == EDIT_JITTER || id == EDIT_CLICK_LIMIT;
}

/**
 * Runs a hotkey's command `act`. When the control with ID `focusedId` is a
 * number field, `apply` takes its typed value first, so a value typed but not
 * yet left counts, as it does when a button is clicked.
 */
template <class Apply, class Act>
void RunHotkey(int focusedId, Apply apply, Act act) {
    if (IsNumberField(focusedId)) apply(focusedId);
    act();
}

}  // namespace flow::ui
