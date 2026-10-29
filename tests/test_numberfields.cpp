/**
 * @file test_numberfields.cpp
 * @brief RunHotkey, which takes the value typed into the focused number field
 * before a global hotkey records, plays, clicks or stops.
 */
#include "doctest.h"

#include "ui/NumberFields.h"

#include <string>
#include <vector>

using namespace flow::ui;

namespace {

/** The steps RunHotkey takes with the focus on `focusedId`, in order. */
std::vector<std::string> steps(int focusedId) {
    std::vector<std::string> out;
    RunHotkey(focusedId,
              [&out](int id) { out.push_back("apply " + std::to_string(id)); },
              [&out] { out.push_back("act"); });
    return out;
}

}  // namespace

TEST_CASE("A hotkey takes the value typed into the focused number field before it acts") {
    for (int id : { EDIT_SPEED, EDIT_LOOPS, EDIT_INTERVAL, EDIT_JITTER, EDIT_CLICK_LIMIT }) {
        CAPTURE(id);
        CHECK(steps(id) == std::vector<std::string>{ "apply " + std::to_string(id), "act" });
    }
}

TEST_CASE("A hotkey only acts when the focus is on a button or outside the window") {
    for (int id : { 0, static_cast<int>(BTN_PLAY), static_cast<int>(BTN_RECORD),
                    static_cast<int>(BTN_TOGGLE_CLICKER), static_cast<int>(CHK_CONTINUOUS) }) {
        CAPTURE(id);
        CHECK(steps(id) == std::vector<std::string>{ "act" });
    }
}
