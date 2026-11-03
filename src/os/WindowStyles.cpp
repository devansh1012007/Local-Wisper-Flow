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

}
