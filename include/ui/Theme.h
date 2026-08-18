/**
 * @file Theme.h
 * @brief FLOW's design system: the palette, the layout grid, and DPI scaling.
 *
 * Every Y coordinate the window paints and every Y coordinate its child
 * controls are created at comes from this one table. Painting and control
 * creation each used to carry their own copy, where a nudge to one drifted
 * silently from the other; there is one source for both now.
 *
 * The numbers are design units at 96 DPI. Run them through Sc()/Scf() at the
 * point of use so the window scales on high-DPI monitors.
 */
#pragma once

#include <windows.h>

namespace flow::ui {

// Refined light palette
const COLORREF BG_PRIMARY      = RGB(243, 245, 249);   // window background
const COLORREF BG_ELEVATED     = RGB(255, 255, 255);   // card / dialog surface
const COLORREF ACCENT_PRIMARY  = RGB(37,  99,  235);   // blue 600
const COLORREF ACCENT_HOVER    = RGB(59,  130, 246);   // blue 500
const COLORREF ACCENT_PRESSED  = RGB(29,  78,  216);   // blue 700
const COLORREF SUCCESS_COLOR   = RGB(22,  163, 74);    // green 600
const COLORREF SUCCESS_HOVER   = RGB(34,  197, 94);    // green 500
const COLORREF DANGER_COLOR    = RGB(220, 38,  38);    // red 600
const COLORREF DANGER_HOVER    = RGB(239, 68,  68);    // red 500
const COLORREF TEXT_PRIMARY    = RGB(17,  24,  39);    // near-black
const COLORREF TEXT_SECONDARY  = RGB(100, 116, 139);   // slate 500
const COLORREF TEXT_FAINT      = RGB(148, 163, 184);   // slate 400
const COLORREF LABEL_GREY      = RGB(140, 140, 148);   // neutral grey (section labels)
const COLORREF STATUS_IDLE     = RGB(124, 138, 160);   // soft slate (idle status dot)
const COLORREF BORDER_DEFAULT  = RGB(226, 232, 240);   // slate 200
const COLORREF TRACK_BG        = RGB(241, 245, 249);   // slate 100 (ghost / input)
const COLORREF TRACK_HOVER     = RGB(226, 232, 240);   // slate 200
const COLORREF SHADOW_COLOR    = RGB(148, 163, 184);
const COLORREF BG_DIALOG       = RGB(255, 255, 255);

// Layout (client-area design units at 96 DPI, single-column workflow).
constexpr int PAD        = 24;
constexpr int CLIENT_W   = 460;
constexpr int CLIENT_H   = 686;
constexpr int CONTENT_X  = PAD;                 // 24
constexpr int CONTENT_W  = CLIENT_W - 2 * PAD;  // 412
constexpr int CRIGHT     = CLIENT_W - PAD;      // 436 (right edge of content)
constexpr int BTN_RADIUS = 10;
constexpr int HERO_H     = 48;                  // primary (hero) button height
constexpr int SEC_BTN_H  = 44;                  // secondary (auto-clicker) button
constexpr int FOOT_H     = 40;                  // footer button height
constexpr int EDIT_W     = 46;
constexpr int CTRL_W     = CONTENT_W - 16;      // controls right-align at CONTENT_X+CTRL_W (=420)
constexpr int CTRL_RIGHT = CONTENT_X + CTRL_W;  // 420

// Vertical rhythm — explicit Y of every element (PaintUI + CreateControls share these)
constexpr int WORDMARK_Y   = 14;
constexpr int SUBTITLE_Y   = 42;

constexpr int REC_DIV_Y    = 66;
constexpr int REC_LABEL_Y  = 80;
constexpr int REC_BTN_Y    = 100;
constexpr int REC_INFO_Y   = 160;

constexpr int PLAY_DIV_Y   = 190;
constexpr int PLAY_LABEL_Y = 204;
constexpr int PLAY_BTN_Y   = 224;
constexpr int ROW_SPEED_Y  = 286;   // label/control row baselines
constexpr int ROW_LOOPS_Y  = 320;
constexpr int ROW_CONT_Y   = 354;
constexpr int ROW_HUM_Y    = 388;
constexpr int RUNTIME_Y    = 420;

constexpr int CLK_DIV_Y     = 444;
constexpr int CLK_LABEL_Y   = 458;
constexpr int CLK_CAPTION_Y = 476;
constexpr int CLK_BTN_Y     = 496;
constexpr int ROW_INTERVAL_Y= 554;
constexpr int INTERVAL_HELP_Y = 582;

constexpr int FOOT_DIV_Y   = 608;
constexpr int FOOT_Y       = 622;

// DPI scale factor (1.0 at 96 DPI), read from the monitor's DPI at startup.
extern double g_scale;

inline int   Sc(int v)    { return (int)(v * g_scale + 0.5); }
inline float Scf(float v) { return (float)(v * g_scale); }

}  // namespace flow::ui
