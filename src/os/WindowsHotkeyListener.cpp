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
std::atomic<bool> g_swallow_enabled{true};
std::atomic<bool> g_avoid_altgr{true};
HHOOK g_hook = nullptr;

bool IsAltGrCombo(const KBDLLHOOKSTRUCT* info) {
    if (!g_avoid_altgr.load(std::memory_order_relaxed)) {
        return false;
    }
    if (info->vkCode != VK_RMENU) {
        return false;
    }
    return (GetKeyState(VK_LCONTROL) & 0x8000) != 0;
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && g_callback) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
        const bool injected = (info->flags & LLKHF_INJECTED) != 0;
        if (!injected && info->vkCode == g_virtual_key.load(std::memory_order_relaxed)) {
            if (IsAltGrCombo(info)) {
                g_key_state.store(-1);
                return CallNextHookEx(nullptr, code, wparam, lparam);
            }
            const bool down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
            const bool up = (wparam == WM_KEYUP || wparam == WM_SYSKEYUP);
            if (down || up) {
                const int next = down ? 1 : 0;
                const int prev = g_key_state.exchange(next);
                if (prev != next) {
                    g_callback(down);
                }
            }
            if (g_swallow_enabled.load(std::memory_order_relaxed)) {
                return 1;
            }
            return CallNextHookEx(nullptr, code, wparam, lparam);
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

}  // namespace

WindowsHotkeyListener::WindowsHotkeyListener(std::uint32_t virtual_key, bool avoid_altgr)
    : virtual_key_(virtual_key), avoid_altgr_(avoid_altgr) {}

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
    g_swallow_enabled.store(swallow_enabled_.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
    g_avoid_altgr.store(avoid_altgr_, std::memory_order_relaxed);
    thread_ = std::thread([this] { ThreadMain(); });
}

void WindowsHotkeyListener::SetSwallowEnabled(bool enabled) {
    swallow_enabled_.store(enabled, std::memory_order_relaxed);
    g_swallow_enabled.store(enabled, std::memory_order_relaxed);
}

bool WindowsHotkeyListener::HookInstalled() const {
    return hook_installed_.load(std::memory_order_relaxed);
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
        hook_installed_.store(false);
        return;
    }
    hook_installed_.store(true);

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
    hook_installed_.store(false);
    thread_id_.store(0);
}
