#include "os/WindowsTextInjector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdexcept>
#include <string>
#include <vector>

void WindowsTextInjector::InjectText(const std::string& text) {
    if (text.empty()) {
        return;
    }

    const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                                static_cast<int>(text.size()), nullptr, 0);
    if (wide_length <= 0) {
        throw std::runtime_error("InjectText: UTF-8 to UTF-16 conversion failed");
    }

    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), &wide[0], wide_length);

    std::vector<INPUT> events;
    events.reserve(wide.size() * 2);

    for (wchar_t character : wide) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(character);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        events.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        events.push_back(up);
    }

    const UINT sent = SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
    if (sent != static_cast<UINT>(events.size())) {
        throw std::runtime_error("InjectText: SendInput failed, error " +
                                 std::to_string(GetLastError()));
    }
}
