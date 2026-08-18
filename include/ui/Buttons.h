/**
 * @file Buttons.h
 * @brief The owner-draw controls: FLOW paints every button itself.
 *
 * Win32's native buttons cannot carry the rounded corners, the accent fills, the
 * vector glyphs or the hotkey hints the design calls for, so each control is
 * created BS_OWNERDRAW and painted here from WM_DRAWITEM. Colours and geometry
 * come from Theme.h; the state each button reflects comes from AppState.h.
 */
#pragma once

#include <windows.h>

namespace flow::ui {

/** Create an owner-draw button, with an optional hover tooltip. */
HWND CreateFlowButton(HWND parent, int id, int x, int y, int w, int h,
                      const wchar_t* tooltip);

/** Paint a main-window button; the look is resolved from its ID and app state. */
void DrawFlowButton(DRAWITEMSTRUCT* dis);

/** Paint a checkbox-style toggle row. */
void DrawToggle(DRAWITEMSTRUCT* dis, const wchar_t* label, bool on);

/** Paint a hotkey capture field showing its current binding. */
void DrawKeyField(DRAWITEMSTRUCT* dis, const wchar_t* keyText);

/** Paint a dialog button — primary is filled, secondary is outlined. */
void DrawDlgButton(DRAWITEMSTRUCT* dis, const wchar_t* label, bool primary);

}  // namespace flow::ui
