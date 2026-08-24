#include "os/WindowsHotkeyListener.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <iostream>
#include <utility>

namespace {

constexpr UINT kWmStopListener = WM_APP + 1;

std::function<void(bool)> g_callback;
std::atomic<std::uint32_t> g_virtual_key{0};
std::atomic<int> g_key_state{-1};
HHOOK g_hook = nullptr;

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && g_callback) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
        const bool injected = (info->flags & LLKHF_INJECTED) != 0;
        if (!injected && info->vkCode == g_virtual_key.load(std::memory_order_relaxed)) {
            const bool down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
            const bool up = (wparam == WM_KEYUP || wparam == WM_SYSKEYUP);
            if (down || up) {
                const int next = down ? 1 : 0;
                const int prev = g_key_state.exchange(next);
                if (prev != next) {
                    g_callback(down);
                }
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

}  // namespace

WindowsHotkeyListener::WindowsHotkeyListener(std::uint32_t virtual_key)
    : virtual_key_(virtual_key) {}

WindowsHotkeyListener::~WindowsHotkeyListener() {
    if (thread_.joinable()) {
        std::uint32_t thread_id = thread_id_.load();
        for (int attempt = 0; attempt < 40 && thread_id != 0; ++attempt) {
            if (PostThreadMessageW(static_cast<DWORD>(thread_id), kWmStopListener, 0, 0)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            thread_id = thread_id_.load();
        }
        thread_.join();
    }
}

void WindowsHotkeyListener::StartListening(std::function<void(bool is_pressed)> callback) {
    if (running_.exchange(true)) {
        return;
    }
    callback_ = std::move(callback);
    g_callback = callback_;
    g_virtual_key.store(virtual_key_);
    g_key_state.store(-1);
    thread_ = std::thread([this] { ThreadMain(); });
}

void WindowsHotkeyListener::ThreadMain() {
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    thread_id_.store(GetCurrentThreadId());

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHookProc, GetModuleHandleW(nullptr), 0);
    if (g_hook == nullptr) {
        std::cerr << "[hotkey] SetWindowsHookEx failed, error " << GetLastError() << '\n';
        thread_id_.store(0);
        running_.store(false);
        return;
    }

    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (message.message == kWmStopListener) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
    g_callback = nullptr;
    thread_id_.store(0);
}
