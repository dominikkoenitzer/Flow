/**
 * @file Dpi.cpp
 * @brief The DPI lookups declared in Dpi.h, resolved at run time.
 */
#include "ui/Dpi.h"

#include "ui/Theme.h"

namespace flow::ui {

namespace {

typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
typedef HRESULT(WINAPI* GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
typedef BOOL(WINAPI* AdjustWindowRectExForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);

FARPROC User32Proc(const char* name) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    return u32 ? GetProcAddress(u32, name) : nullptr;
}

double ScaleFor(UINT dpi) {
    double s = dpi / 96.0;
    return s < 1.0 ? 1.0 : s;
}

}  // namespace

UINT SystemDpi() {
    HDC screen = GetDC(NULL);
    int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 0;
    if (screen) ReleaseDC(NULL, screen);
    return dpi > 0 ? (UINT)dpi : 96;
}

UINT CurrentDpi() {
    return (UINT)(g_scale * 96.0 + 0.5);
}

void SetScaleFromDpi(UINT dpi) {
    g_scale = ScaleFor(dpi);
}

int ScAt(int v, UINT dpi) {
    return (int)(v * ScaleFor(dpi) + 0.5);
}

UINT DpiForWindow(HWND hwnd) {
    static auto fn = (GetDpiForWindowFn)(void*)User32Proc("GetDpiForWindow");
    UINT dpi = (fn && hwnd) ? fn(hwnd) : 0;
    return dpi ? dpi : SystemDpi();
}

UINT DpiForRect(const RECT& rc) {
    // shcore is loaded once and kept for the life of the process.
    static HMODULE shcore = LoadLibraryW(L"shcore.dll");
    static auto fn = shcore ?
        (GetDpiForMonitorFn)(void*)GetProcAddress(shcore, "GetDpiForMonitor") : nullptr;
    HMONITOR mon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    UINT dx = 0, dy = 0;
    if (fn && mon && SUCCEEDED(fn(mon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy)) && dx)
        return dx;
    return SystemDpi();
}

SIZE WindowSizeForClient(int w, int h, DWORD style, DWORD exStyle, UINT dpi) {
    static auto fn = (AdjustWindowRectExForDpiFn)(void*)User32Proc("AdjustWindowRectExForDpi");
    RECT r = { 0, 0, w, h };
    if (!fn || !fn(&r, style, FALSE, exStyle, dpi))
        AdjustWindowRectEx(&r, style, FALSE, exStyle);
    return SIZE{ r.right - r.left, r.bottom - r.top };
}

}  // namespace flow::ui
