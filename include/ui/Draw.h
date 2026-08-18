/**
 * @file Draw.h
 * @brief Anti-aliased GDI+ primitives and the vector glyphs on the buttons.
 *
 * The icons are drawn rather than loaded so they stay crisp at any DPI and the
 * executable carries no bitmap resources. Each takes a centre and a half-extent
 * so callers place them by the middle of a button, not by a corner.
 */
#pragma once

#include <windows.h>
#include <gdiplus.h>

namespace flow::ui {

/** COLORREF to a GDI+ colour, with an optional alpha. */
Gdiplus::Color GP(COLORREF c, BYTE a = 255);

void AddRoundRectPath(Gdiplus::GraphicsPath& path, float x, float y,
                      float w, float h, float r);
void FillRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
               float r, Gdiplus::Color fill);
void StrokeRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
                 float r, Gdiplus::Color stroke, float width = 1.0f);

// Vector glyphs, centred at (cx, cy) with half-extent s.
void IconCircle(Gdiplus::Graphics& g, float cx, float cy, float r, Gdiplus::Color c);
void IconTriangle(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c);
void IconSquare(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c);
void IconBolt(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c);

}  // namespace flow::ui
