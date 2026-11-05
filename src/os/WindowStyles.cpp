#include "os/WindowStyles.h"

#ifdef _WIN32
#include <windows.h>
#else
#endif

namespace WindowStyles {

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

}  // namespace WindowStyles
