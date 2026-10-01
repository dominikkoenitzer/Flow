/**
 * @file test_placement.cpp
 * @brief ClampIntoArea, which keeps a restored window inside its monitor's
 * work area.
 *
 * The window is roughly the main one at 100%: its 460 x 776 client area plus
 * a frame. The work areas are what Windows reports with the taskbar on each
 * side of the monitor.
 */
#include "doctest.h"

#include "ui/Placement.h"

#include <climits>

using flow::ui::ClampIntoArea;
using flow::ui::RectOnDesktop;

namespace {

constexpr int W = 476;
constexpr int H = 815;

RECT area(LONG left, LONG top, LONG right, LONG bottom) {
    RECT r;
    r.left = left;
    r.top = top;
    r.right = right;
    r.bottom = bottom;
    return r;
}

}  // namespace

TEST_CASE("A window that fits the work area keeps its place") {
    const RECT work = area(0, 0, 1920, 1032);
    const POINT p = ClampIntoArea(300, 100, W, H, work);
    CHECK(p.x == 300);
    CHECK(p.y == 100);

    // Touching every edge is still inside.
    const POINT corner = ClampIntoArea(1920 - W, 1032 - H, W, H, work);
    CHECK(corner.x == 1920 - W);
    CHECK(corner.y == 1032 - H);
}

TEST_CASE("A taskbar at the bottom lifts the window's foot above it") {
    // 1080p with a 48 px taskbar: the work area ends at 1032.
    const RECT work = area(0, 0, 1920, 1032);
    const POINT p = ClampIntoArea(300, 260, W, H, work);
    CHECK(p.x == 300);
    CHECK(p.y == 1032 - H);
}

TEST_CASE("A taskbar on the left pushes the window right of it") {
    const RECT work = area(62, 0, 1920, 1080);
    const POINT p = ClampIntoArea(0, 100, W, H, work);
    CHECK(p.x == 62);
    CHECK(p.y == 100);
}

TEST_CASE("A taskbar at the top pushes the window below it") {
    const RECT work = area(0, 48, 1920, 1080);
    const POINT p = ClampIntoArea(300, 10, W, H, work);
    CHECK(p.x == 300);
    CHECK(p.y == 48);
}

TEST_CASE("A taskbar on the right pulls the window left of it") {
    // The window would fit the 1920 px screen, but its right edge lies under
    // a 62 px taskbar.
    const RECT work = area(0, 0, 1858, 1080);
    const POINT p = ClampIntoArea(1400, 100, W, H, work);
    CHECK(p.x == 1858 - W);
    CHECK(p.y == 100);
}

TEST_CASE("A window past the right edge is pulled back inside") {
    const RECT work = area(0, 0, 1920, 1032);
    const POINT p = ClampIntoArea(1800, 100, W, H, work);
    CHECK(p.x == 1920 - W);
    CHECK(p.y == 100);
}

TEST_CASE("A window larger than the work area is pinned to its top-left") {
    // 1366 x 768 with a 40 px taskbar is shorter than the window.
    const RECT work = area(0, 0, 1366, 728);
    const POINT tall = ClampIntoArea(200, 300, W, H, work);
    CHECK(tall.x == 200);
    CHECK(tall.y == 0);

    // Too wide and too tall: the caption's left end stays on screen.
    const POINT both = ClampIntoArea(-50, 500, 1400, H, work);
    CHECK(both.x == 0);
    CHECK(both.y == 0);

    // Pinned to an offset work area, not to zero.
    const POINT offset = ClampIntoArea(900, 900, W, H, area(62, 48, 1366, 768));
    CHECK(offset.x == 1366 - W);
    CHECK(offset.y == 48);
}

TEST_CASE("Negative coordinates on a monitor left of the primary are kept") {
    // A 1920 x 1080 monitor left of the primary, taskbar at its bottom.
    const RECT work = area(-1920, 0, 0, 1032);

    const POINT inside = ClampIntoArea(-1500, 100, W, H, work);
    CHECK(inside.x == -1500);
    CHECK(inside.y == 100);

    const POINT low = ClampIntoArea(-1500, 400, W, H, work);
    CHECK(low.x == -1500);
    CHECK(low.y == 1032 - H);

    // Hanging over the primary's edge: pulled back onto the left monitor.
    const POINT overRight = ClampIntoArea(-200, 100, W, H, work);
    CHECK(overRight.x == -W);
    CHECK(overRight.y == 100);

    const POINT overLeft = ClampIntoArea(-2100, -30, W, H, work);
    CHECK(overLeft.x == -1920);
    CHECK(overLeft.y == 0);
}

TEST_CASE("A left monitor set higher than the primary keeps its negative top") {
    // Its top edge sits 300 px above the primary's, taskbar on its left.
    const RECT work = area(-1920 + 48, -300, 0, 780);

    const POINT top = ClampIntoArea(-1000, -400, W, H, work);
    CHECK(top.x == -1000);
    CHECK(top.y == -300);

    const POINT bottomLeft = ClampIntoArea(-1920, 100, W, H, work);
    CHECK(bottomLeft.x == -1920 + 48);
    CHECK(bottomLeft.y == 780 - H);
}

TEST_CASE("A saved position at the ends of int gives a rectangle on the desktop") {
    // settings.cfg is plain text, and atoi turns a huge number into INT_MAX or
    // INT_MIN. Two monitors side by side, the left one at negative x.
    const RECT desktop = area(-1920, 0, 1920, 1080);

    const RECT atMax = RectOnDesktop(INT_MAX, INT_MAX, W, H, desktop);
    CHECK(atMax.left == 1920 - W);
    CHECK(atMax.top == 1080 - H);
    CHECK(atMax.right == 1920);
    CHECK(atMax.bottom == 1080);

    const RECT atMin = RectOnDesktop(INT_MIN, INT_MIN, W, H, desktop);
    CHECK(atMin.left == -1920);
    CHECK(atMin.top == 0);
    CHECK(atMin.right == -1920 + W);
    CHECK(atMin.bottom == H);

    // A position already on the desktop is kept as it is.
    const RECT kept = RectOnDesktop(-1500, 100, W, H, desktop);
    CHECK(kept.left == -1500);
    CHECK(kept.top == 100);
    CHECK(kept.right == -1500 + W);
    CHECK(kept.bottom == 100 + H);
}
