#include "os/WindowsTextInjector.h"

#include "core/Utf8.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kBatchEvents = 64;
constexpr std::chrono::milliseconds kBatchDelay{4};

std::wstring ToWide(const std::string& text) {
    const std::string safe = Utf8::TrimTrailingPartial(text);
    if (safe.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, safe.data(),
                                           static_cast<int>(safe.size()), nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("InjectText: UTF-8 to UTF-16 conversion failed");
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, safe.data(), static_cast<int>(safe.size()), &wide[0],
                        length);
    return wide;
}

}  // namespace

void WindowsTextInjector::InjectText(const std::string& text) {
    if (text.empty()) {
        return;
    }

    std::wstring wide;
    try {
        wide = ToWide(text);
    } catch (const std::exception&) {
        wide = std::wstring(text.begin(), text.end());
    }

    std::vector<INPUT> events;
    events.reserve(wide.size() * 2);
    for (wchar_t character : wide) {
        if (character == L'\n') {
            character = L'\r';
        }
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(character);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        events.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        events.push_back(up);
    }

    UINT offset = 0;
    while (offset < events.size()) {
        const UINT count =
            static_cast<UINT>(std::min(kBatchEvents, events.size() - offset));
        const UINT sent = SendInput(count, events.data() + offset, sizeof(INPUT));
        if (sent != count) {
            throw std::runtime_error("InjectText: SendInput failed, error " +
                                     std::to_string(GetLastError()));
        }
        offset += count;
        if (offset < events.size()) {
            std::this_thread::sleep_for(kBatchDelay);
        }
    }
}
