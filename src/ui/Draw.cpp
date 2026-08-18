/**
 * @file Draw.cpp
 * @brief Implementations of the GDI+ primitives declared in Draw.h.
 */
#include "ui/Draw.h"

namespace flow::ui {

Gdiplus::Color GP(COLORREF c, BYTE a) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

void AddRoundRectPath(Gdiplus::GraphicsPath& path, float x, float y,
                             float w, float h, float r) {
    float d = r * 2.0f;
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void FillRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
                      float r, Gdiplus::Color fill) {
    Gdiplus::GraphicsPath path;
    AddRoundRectPath(path, x, y, w, h, r);
    Gdiplus::SolidBrush brush(fill);
    g.FillPath(&brush, &path);
}

void StrokeRound(Gdiplus::Graphics& g, float x, float y, float w, float h,
                        float r, Gdiplus::Color stroke, float width) {
    Gdiplus::GraphicsPath path;
    // Inset by half the pen width so the stroke stays inside the rect
    AddRoundRectPath(path, x + width / 2, y + width / 2, w - width, h - width, r);
    Gdiplus::Pen pen(stroke, width);
    g.DrawPath(&pen, &path);
}

// ---- Vector glyphs (centered at cx,cy, half-extent s) ----
void IconCircle(Gdiplus::Graphics& g, float cx, float cy, float r, Gdiplus::Color c) {
    Gdiplus::SolidBrush b(c);
    g.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}
void IconTriangle(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(cx - s * 0.55f, cy - s),
        Gdiplus::PointF(cx - s * 0.55f, cy + s),
        Gdiplus::PointF(cx + s * 0.9f,  cy)
    };
    Gdiplus::SolidBrush b(c);
    g.FillPolygon(&b, pts, 3);
}
void IconSquare(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    FillRound(g, cx - s, cy - s, s * 2, s * 2, 3.0f, c);
}
void IconBolt(Gdiplus::Graphics& g, float cx, float cy, float s, Gdiplus::Color c) {
    float u = s / 8.0f;
    Gdiplus::PointF pts[6] = {
        Gdiplus::PointF(cx + 1.0f * u, cy - 8.0f * u),
        Gdiplus::PointF(cx - 4.0f * u, cy + 1.0f * u),
        Gdiplus::PointF(cx - 0.5f * u, cy + 1.0f * u),
        Gdiplus::PointF(cx - 1.5f * u, cy + 8.0f * u),
        Gdiplus::PointF(cx + 4.0f * u, cy - 2.0f * u),
        Gdiplus::PointF(cx + 0.5f * u, cy - 2.0f * u)
    };
    Gdiplus::SolidBrush b(c);
    g.FillPolygon(&b, pts, 6);
}

}  // namespace flow::ui
