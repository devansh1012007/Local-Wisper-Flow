#include "os/WindowStyles.h"

#ifdef _WIN32
#include <windows.h>
#else
#endif

namespace WindowStyles {

#ifdef _WIN32
namespace {

// Resource id of the exe icon (assets/app.rc.in -> EchoFlow.rc). Keep in
// sync with WindowsTray.cpp, which loads the same handle for the tray.
constexpr wchar_t kAppIconId = 1;

HICON LoadAppIcon(int width, int height) {
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                         MAKEINTRESOURCEW(kAppIconId), IMAGE_ICON,
                                         width, height, LR_DEFAULTCOLOR));
}

}  // namespace
#endif

void ApplyAppIcon(void* hwnd) {
#ifdef _WIN32
    if (hwnd == nullptr) {
        return;
    }
    const HWND window = static_cast<HWND>(hwnd);
    const int small_w = GetSystemMetrics(SM_CXSMICON);
    const int small_h = GetSystemMetrics(SM_CYSMICON);
    const int big_w = GetSystemMetrics(SM_CXICON);
    const int big_h = GetSystemMetrics(SM_CYICON);
    if (HICON small_icon = LoadAppIcon(small_w, small_h); small_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
    if (HICON big_icon = LoadAppIcon(big_w, big_h); big_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
    }
#else
    (void)hwnd;
#endif
}

void ApplyOverlayStyles(void* hwnd) {
#ifdef _WIN32
    if (hwnd == nullptr) {
        return;
    }
    const HWND window = static_cast<HWND>(hwnd);
    const LONG styles = GetWindowLongW(window, GWL_EXSTYLE) | WS_EX_NOACTIVATE |
                        WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
    SetWindowLongW(window, GWL_EXSTYLE, styles);
#else
    (void)hwnd;
#endif
}

void SetOverlayPassThrough(void* hwnd, bool pass_through) {
#ifdef _WIN32
    if (hwnd == nullptr) {
        return;
    }
    const HWND window = static_cast<HWND>(hwnd);
    const LONG current = GetWindowLongW(window, GWL_EXSTYLE);
    const LONG updated = pass_through ? (current | WS_EX_TRANSPARENT)
                                      : (current & ~static_cast<LONG>(WS_EX_TRANSPARENT));
    if (updated != current) {
        SetWindowLongW(window, GWL_EXSTYLE, updated);
    }
#else
    (void)pass_through;
    (void)hwnd;
#endif
}

bool IsCursorInsideRect(int x, int y, int width, int height, int inflate) {
#ifdef _WIN32
    POINT cursor{};
    if (GetCursorPos(&cursor) == 0) {
        return false;
    }
    const RECT zone{ x - inflate, y - inflate, x + width + inflate, y + height + inflate };
    return PtInRect(&zone, cursor) != 0;
#else
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)inflate;
    return false;
#endif
}

bool CursorScreenPoint(int& x, int& y) {
#ifdef _WIN32
    POINT cursor{};
    if (GetCursorPos(&cursor) == 0) {
        return false;
    }
    x = cursor.x;
    y = cursor.y;
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}

bool LeftMouseButtonDown() {
#ifdef _WIN32
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#else
    return false;
#endif
}

bool PrimaryWorkArea(int& x, int& y, int& width, int& height) {
#ifdef _WIN32
    RECT work{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        x = work.left;
        y = work.top;
        width = work.right - work.left;
        height = work.bottom - work.top;
        return true;
    }
    return false;
#else
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return false;
#endif
}

}  // namespace WindowStyles
