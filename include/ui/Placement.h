/**
 * @file Placement.h
 * @brief Where a restored window may sit on its monitor.
 *
 * Plain arithmetic on rectangles with no Windows calls, so the tests can check
 * it without a window or a monitor.
 */
#pragma once

#include <windows.h>

namespace flow::ui {

/**
 * Top-left corner for a w x h window at (x, y), moved by as little as it takes
 * to lie inside `area`. A window that already fits keeps its place. One wider
 * or taller than the area is pinned to the area's left or top edge, so its
 * caption stays reachable.
 */
inline POINT ClampIntoArea(int x, int y, int w, int h, const RECT& area) {
    if (x > area.right - w) x = area.right - w;
    if (x < area.left) x = area.left;
    if (y > area.bottom - h) y = area.bottom - h;
    if (y < area.top) y = area.top;
    POINT p;
    p.x = x;
    p.y = y;
    return p;
}

/**
 * The w x h rectangle at (x, y), moved onto `desktop` first. A saved position
 * comes from settings.cfg, which can hold any int, and adding the size to one
 * near INT_MAX would overflow. Once the corner lies on the desktop the sums
 * cannot.
 */
inline RECT RectOnDesktop(int x, int y, int w, int h, const RECT& desktop) {
    const POINT p = ClampIntoArea(x, y, w, h, desktop);
    RECT r;
    r.left = p.x;
    r.top = p.y;
    r.right = p.x + w;
    r.bottom = p.y + h;
    return r;
}

}  // namespace flow::ui
