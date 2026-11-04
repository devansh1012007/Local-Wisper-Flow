#include "os/WindowsTray.h"

#include "core/Log.h"
#include "core/SettingsStore.h"
#include "core/UiBus.h"

#include <windows.h>
#include <shellapi.h>

#include <condition_variable>
#include <mutex>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

constexpr UINT kWmTrayIcon = WM_APP + 7;
constexpr UINT kWmOpenSettingsEvent = WM_APP + 8;
constexpr int kIconId = 1;
constexpr int kMenuEnable = 1001;
constexpr int kMenuSettings = 1002;
constexpr int kMenuHistory = 1003;
constexpr int kMenuLogs = 1004;
constexpr int kMenuAutostart = 1005;
constexpr int kMenuQuit = 1006;

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"LocalWisperFlow";

}

struct WindowsTray::Impl {
    UiBus& bus;
    SettingsStore& settings;
    RateLimiter notify_limiter{10000};
    std::thread thread;
    std::atomic<bool> running{false};
    HWND window = nullptr;
    NOTIFYICONDATAW icon_data{};
    std::mutex mutex;

    explicit Impl(UiBus& bus_ref, SettingsStore& settings_ref) : bus(bus_ref), settings(settings_ref) {}

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void ShowContextMenu();
    void ReaddIcon();
};

LRESULT CALLBACK WindowsTray::Impl::WindowProc(HWND window, UINT message, WPARAM wParam,
                                               LPARAM lParam) {
    WindowsTray::Impl* self = reinterpret_cast<WindowsTray::Impl*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    if (message == kWmTrayIcon) {
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            self->ShowContextMenu();
        } else if (LOWORD(lParam) == WM_LBUTTONUP) {
            self->bus.PostCommand(UiCommandType::OpenSettings);
        }
        return 0;
    }
    if (message == WM_COMMAND) {
        const int command = LOWORD(wParam);
        switch (command) {
            case kMenuEnable: {
                const bool now_enabled = self->settings.GetBool("dictation_enabled") == false;
                self->settings.SetBool("dictation_enabled", now_enabled);
                self->bus.PostCommand(UiCommandType::SetEnabled, now_enabled ? "1" : "0");
                break;
            }
            case kMenuSettings:
                self->bus.PostCommand(UiCommandType::OpenSettings);
                break;
            case kMenuHistory:
                self->bus.PostCommand(UiCommandType::OpenHistory);
                break;
            case kMenuLogs:
                self->bus.PostCommand(UiCommandType::OpenLogsFolder);
                break;
            case kMenuAutostart:
                Autostart::SetEnabled(!Autostart::IsEnabled());
                break;
            case kMenuQuit:
                self->bus.PostCommand(UiCommandType::Quit);
                break;
            default:
                break;
        }
        return 0;
    }
    if (message == kWmOpenSettingsEvent) {
        self->bus.PostCommand(UiCommandType::OpenSettings);
        return 0;
    }
    if (message == WM_DESTROY) {
        Shell_NotifyIconW(NIM_DELETE, &self->icon_data);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void WindowsTray::Impl::ShowContextMenu() {
    POINT point{};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    const bool enabled = settings.GetBool("dictation_enabled");
    AppendMenuW(menu, MF_STRING | (enabled ? MF_CHECKED : 0), kMenuEnable, L"Enable dictation");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenuW(menu, MF_STRING, kMenuHistory, L"History...");
    AppendMenuW(menu, MF_STRING, kMenuLogs, L"Open logs folder");
    AppendMenuW(menu, MF_STRING | (Autostart::IsEnabled() ? MF_CHECKED : 0), kMenuAutostart,
                L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, L"Quit");
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
}

void WindowsTray::Impl::ReaddIcon() {
    Shell_NotifyIconW(NIM_DELETE, &icon_data);
    Shell_NotifyIconW(NIM_ADD, &icon_data);
}

namespace Autostart {

bool IsEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    BYTE buffer[MAX_PATH * 2];
    DWORD size = sizeof(buffer);
    const LRESULT result =
        RegQueryValueExW(key, kRunValue, nullptr, &type, buffer, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ;
}

bool SetEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    LRESULT result = enabled
                               ? RegSetValueExW(key, kRunValue, 0, REG_SZ,
                                                reinterpret_cast<const BYTE*>(L"\""),
                                                0)
                               : RegDeleteValueW(key, kRunValue);
    if (enabled && result == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring command = L"\"" + std::wstring(path) + L"\"";
        result = RegSetValueExW(key, kRunValue, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

}

WindowsTray::WindowsTray(UiBus& bus, SettingsStore& settings)
    : impl_(std::make_unique<Impl>(bus, settings)) {}

WindowsTray::~WindowsTray() {
    Stop();
}

void WindowsTray::Start() {
    if (impl_->running.exchange(true)) {
        return;
    }
    impl_->thread = std::thread([this] {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &Impl::WindowProc;
        window_class.lpszClassName = L"LwfTrayWindow";
        window_class.hInstance = GetModuleHandleW(nullptr);
        RegisterClassW(&window_class);
        impl_->window = CreateWindowExW(0, window_class.lpszClassName, L"", 0, 0, 0, 0, 0,
                                        HWND_MESSAGE, nullptr, window_class.hInstance, nullptr);
        SetWindowLongPtrW(impl_->window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));

        NOTIFYICONDATAW& data = impl_->icon_data;
        data.cbSize = sizeof(data);
        data.hWnd = impl_->window;
        data.uID = kIconId;
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        data.uCallbackMessage = kWmTrayIcon;
        data.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        wcscpy_s(data.szTip, L"Local Wisper Flow");
        Shell_NotifyIconW(NIM_ADD, &data);

        UINT taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

        MSG message;
        BOOL has_message = GetMessageW(&message, nullptr, 0, 0);
        while (has_message > 0 && impl_->running.load()) {
            if (message.message == taskbar_created) {
                impl_->ReaddIcon();
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
            has_message = GetMessageW(&message, nullptr, 0, 0);
        }
    });
}

void WindowsTray::Stop() {
    if (!impl_->running.exchange(false)) {
        return;
    }
    if (impl_->window != nullptr) {
        PostMessageW(impl_->window, WM_CLOSE, 0, 0);
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

void WindowsTray::NotifyError(ErrorCode code, const std::string& detail) {
    const std::int64_t now = GetTickCount64();
    if (!impl_->notify_limiter.Allow(code, now)) {
        return;
    }
    NOTIFYICONDATAW& data = impl_->icon_data;
    data.uFlags = NIF_INFO;
    const std::wstring wide(detail.begin(), detail.end());
    wcsncpy_s(data.szInfo, wide.c_str(), _TRUNCATE);
    wcsncpy_s(data.szInfoTitle, L"Local Wisper Flow", _TRUNCATE);
    data.dwInfoFlags = NIIF_ERROR;
    Shell_NotifyIconW(NIM_MODIFY, &data);
    lwf::Log::Error("tray notify: " + std::string(ToString(code)) + " " + detail);
}
