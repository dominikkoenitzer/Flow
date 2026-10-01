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

}  // namespace flow::ui
