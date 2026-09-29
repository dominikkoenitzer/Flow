/**
 * @file Dpi.h
 * @brief Per-monitor DPI lookups, and the frame size for a client area at a DPI.
 *
 * FLOW is Per-Monitor V2 aware, so each window is laid out for the monitor it
 * is on and relaid when it moves to one with another scale. The Windows calls
 * behind this only exist on Windows 10 and 8.1, so each is resolved at run
 * time and falls back to the system DPI where it is missing.
 */
#pragma once

#include <windows.h>

// Newer than the headers' default WINVER; the value is fixed by Windows.
#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

namespace flow::ui {

/** The system DPI (the primary monitor's at sign-in). */
UINT SystemDpi();

/** The DPI g_scale stands for right now. */
UINT CurrentDpi();

/** Set g_scale for a DPI (96 is 1.0; never below 1.0). */
void SetScaleFromDpi(UINT dpi);

/** Sc() at a DPI other than the current one. */
int ScAt(int v, UINT dpi);

/** DPI of the monitor a window is on. */
UINT DpiForWindow(HWND hwnd);

/** DPI of the monitor that holds most of a rectangle, or the nearest one. */
UINT DpiForRect(const RECT& rc);

/** Outer size of a window with this style whose client area is w x h at `dpi`. */
SIZE WindowSizeForClient(int w, int h, DWORD style, DWORD exStyle, UINT dpi);

}  // namespace flow::ui
