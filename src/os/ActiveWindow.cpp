#include "core/IActiveWindow.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#endif

#include <algorithm>

namespace {

std::string ToLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

}

#ifdef _WIN32

class WindowsActiveWindow : public IActiveWindow {
public:
    ActiveWindowInfo Get() override {
        ActiveWindowInfo info;
        HWND foreground = GetForegroundWindow();
        if (foreground == nullptr) {
            return info;
        }
        wchar_t title[512];
        const int title_len = GetWindowTextW(foreground, title, 512);
        if (title_len > 0) {
            const int utf8_len = WideCharToMultiByte(CP_UTF8, 0, title, title_len, nullptr, 0,
                                                     nullptr, nullptr);
            if (utf8_len > 0) {
                std::string utf8(static_cast<std::size_t>(utf8_len), '\0');
                WideCharToMultiByte(CP_UTF8, 0, title, title_len, &utf8[0], utf8_len, nullptr,
                                    nullptr);
                info.window_title = std::move(utf8);
            }
        }

        DWORD process_id = 0;
        GetWindowThreadProcessId(foreground, &process_id);
        if (process_id == 0) {
            return info;
        }
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
        if (process == nullptr) {
            return info;
        }
        wchar_t path[1024];
        DWORD path_size = 1024;
        if (QueryFullProcessImageNameW(process, 0, path, &path_size) && path_size > 0) {
            std::wstring wide(path, path_size);
            const std::size_t slash = wide.find_last_of(L"/\\");
            const std::wstring name =
                slash == std::wstring::npos ? wide : wide.substr(slash + 1);
            const int utf8_len = WideCharToMultiByte(
                CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()), nullptr, 0, nullptr,
                nullptr);
            if (utf8_len > 0) {
                std::string utf8(static_cast<std::size_t>(utf8_len), '\0');
                WideCharToMultiByte(CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()),
                                    &utf8[0], utf8_len, nullptr, nullptr);
                info.process_name = std::move(utf8);
            }
        }
        CloseHandle(process);
        return info;
    }
};

IActiveWindow* CreateActiveWindow() {
    static WindowsActiveWindow instance;
    return &instance;
}

#else

class PosixActiveWindow : public IActiveWindow {
public:
    ActiveWindowInfo Get() override { return {}; }
};

IActiveWindow* CreateActiveWindow() {
    static PosixActiveWindow instance;
    return &instance;
}

#endif
