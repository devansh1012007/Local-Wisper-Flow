#include "os/WindowsHotkeyListener.h"

#include <windows.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr UINT kWmAppLearnAbort = WM_APP + 41;
constexpr UINT kWmAppSessionUnlock = WM_APP + 42;
constexpr UINT kWmAppReinstallHook = WM_APP + 43;
constexpr UINT kUnassignedVk = 0xE7;

template <typename T, std::size_t Capacity>
class SpscRing {
public:
    bool Push(const T& value) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        slots_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool Pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        out = slots_[tail];
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> slots_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

bool RequiresAltGrLayout() {
    const HKL layout = GetKeyboardLayout(0);
    const SHORT scanned = VkKeyScanExW(L'{', layout);
    if (scanned == -1) {
        return false;
    }
    const SHORT shifted = VkKeyScanExW(L'-', layout);
    (void)shifted;
    const UINT shift_state = (scanned >> 8) & 0xFF;
    return (shift_state & 0x06) == 0x06;
}

}

struct WindowsHotkeyListener::Impl {
    HotkeyTracker tracker;
    SpscRing<EngineEvent, 256> events;
    std::atomic<bool> running{false};
    std::atomic<bool> hook_installed{false};
    std::atomic<bool> learn_mode{false};
    std::atomic<bool> learned_ready{false};
    std::string learned_binding;
    std::vector<std::uint32_t> learned_keys;
    std::thread thread;
    std::atomic<std::uint32_t> thread_id{0};
    HWND message_window = nullptr;
    HHOOK hook = nullptr;
    std::mutex tracker_mutex;
    std::atomic<bool> suppress_next_win_up{false};

    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam);
    static Impl* instance();
    void PumpMessages();
    bool HandleLearnMode(const KBDLLHOOKSTRUCT* info, bool is_down);
    LRESULT DispatchKey(bool is_down, const KBDLLHOOKSTRUCT* info);
    void ReinstallHook();
};

WindowsHotkeyListener::Impl* g_listener_instance = nullptr;

LRESULT CALLBACK WindowsHotkeyListener::Impl::HookProc(int code, WPARAM wParam,
                                                       LPARAM lParam) {
    if (code != HC_ACTION) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (info == nullptr) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    const bool is_down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool is_up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!is_down && !is_up) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    return instance()->DispatchKey(is_down, info);
}

WindowsHotkeyListener::Impl* WindowsHotkeyListener::Impl::instance() {
    return g_listener_instance;
}

LRESULT WindowsHotkeyListener::Impl::DispatchKey(bool is_down,
                                                 const KBDLLHOOKSTRUCT* info) {
    RawKeyEvent event;
    event.vk = static_cast<std::uint32_t>(info->vkCode);
    event.is_down = is_down;
    event.injected = (info->flags & LLKHF_INJECTED) != 0;
    event.timestamp_ms = static_cast<std::int64_t>(info->time);

    if (HandleLearnMode(info, is_down)) {
        return 1;
    }

    TrackerResult result;
    {
        std::lock_guard<std::mutex> lock(tracker_mutex);
        result = tracker.Feed(event);
    }

    for (const EngineEvent& engine_event : result.events) {
        events.Push(engine_event);
    }

    const bool win_involved = event.vk == HotkeyBinding::kWin;
    if (win_involved && is_down && result.swallow) {
        suppress_next_win_up.store(true);
    }
    if (win_involved && !is_down && result.swallow && suppress_next_win_up.load()) {
        suppress_next_win_up.store(false);
        INPUT dummy{};
        dummy.type = INPUT_KEYBOARD;
        dummy.ki.wVk = kUnassignedVk;
        dummy.ki.dwFlags = 0;
        INPUT dummy_up = dummy;
        dummy_up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &dummy, sizeof(INPUT));
        SendInput(1, &dummy_up, sizeof(INPUT));
    }

    return result.swallow ? 1 : CallNextHookEx(nullptr, 0, is_down ? WM_KEYDOWN : WM_KEYUP,
                                               reinterpret_cast<LPARAM>(info));
}

bool WindowsHotkeyListener::Impl::HandleLearnMode(const KBDLLHOOKSTRUCT* info,
                                                  bool is_down) {
    if (!learn_mode.load()) {
        return false;
    }
    const std::uint32_t vk = static_cast<std::uint32_t>(info->vkCode);
    if (is_down) {
        if (std::find(learned_keys.begin(), learned_keys.end(), vk) == learned_keys.end()) {
            learned_keys.push_back(vk);
        }
    } else {
        if (!learned_keys.empty()) {
            Binding binding;
            binding.keys = learned_keys;
            binding.role = BindingRole::PushToTalk;
            learned_binding = HotkeyBinding::Serialize(binding);
            learned_ready.store(true);
            learn_mode.store(false);
            learned_keys.clear();
        }
    }
    return true;
}

LRESULT CALLBACK WindowsHotkeyListener::Impl::WindowProc(HWND window, UINT message,
                                                         WPARAM wParam, LPARAM lParam) {
    if (message == kWmAppSessionUnlock || message == kWmAppReinstallHook) {
        instance()->ReinstallHook();
        return 0;
    }
    if (message == WM_WTSSESSION_CHANGE && wParam == WTS_SESSION_UNLOCK) {
        instance()->ReinstallHook();
        return 0;
    }
    if (message == WM_POWERBROADCAST && wParam == PBT_APMRESUMEAUTOMATIC) {
        instance()->ReinstallHook();
        return TRUE;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void WindowsHotkeyListener::Impl::ReinstallHook() {
    if (hook != nullptr) {
        UnhookWindowsHookEx(hook);
        hook = nullptr;
    }
    hook = SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::HookProc, GetModuleHandleW(nullptr), 0);
    hook_installed.store(hook != nullptr);
}

void WindowsHotkeyListener::Impl::PumpMessages() {
    MSG message;
    BOOL has_message = PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
    while (has_message) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        has_message = PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
    }
}

WindowsHotkeyListener::WindowsHotkeyListener(std::vector<Binding> bindings,
                                             HotkeyTracker::Options options)
    : impl_(std::make_unique<Impl>()) {
    impl_->tracker.SetBindings(std::move(bindings));
    impl_->tracker.SetOptions(options);
    g_listener_instance = impl_.get();
}

WindowsHotkeyListener::~WindowsHotkeyListener() {
    Stop();
    if (g_listener_instance == impl_.get()) {
        g_listener_instance = nullptr;
    }
}

void WindowsHotkeyListener::Start() {
    if (impl_->running.exchange(true)) {
        return;
    }
    impl_->thread = std::thread([this] {
        impl_->thread_id.store(GetCurrentThreadId());
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &Impl::WindowProc;
        window_class.lpszClassName = L"LwfHotkeyListenerWindow";
        window_class.hInstance = GetModuleHandleW(nullptr);
        RegisterClassW(&window_class);
        impl_->message_window =
            CreateWindowExW(0, window_class.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, window_class.hInstance, nullptr);
        WTSRegisterSessionNotification(impl_->message_window, NOTIFY_FOR_THIS_SESSION);
        impl_->ReinstallHook();

        while (impl_->running.load()) {
            impl_->PumpMessages();
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 50, QS_ALLINPUT);
        }

        WTSUnRegisterSessionNotification(impl_->message_window);
        if (impl_->hook != nullptr) {
            UnhookWindowsHookEx(impl_->hook);
            impl_->hook = nullptr;
        }
        impl_->hook_installed.store(false);
        DestroyWindow(impl_->message_window);
        impl_->message_window = nullptr;
    });
}

void WindowsHotkeyListener::Stop() {
    if (!impl_->running.exchange(false)) {
        return;
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

bool WindowsHotkeyListener::PollEvent(EngineEvent& out) {
    return impl_->events.Pop(out);
}

void WindowsHotkeyListener::SetCancelArmed(bool armed) {
    std::lock_guard<std::mutex> lock(impl_->tracker_mutex);
    impl_->tracker.SetCancelArmed(armed);
}

void WindowsHotkeyListener::UpdateBindings(std::vector<Binding> bindings) {
    std::lock_guard<std::mutex> lock(impl_->tracker_mutex);
    impl_->tracker.SetBindings(std::move(bindings));
}

void WindowsHotkeyListener::SetTrackerOptions(const HotkeyTracker::Options& options) {
    std::lock_guard<std::mutex> lock(impl_->tracker_mutex);
    impl_->tracker.SetOptions(options);
}

void WindowsHotkeyListener::BeginLearnMode() {
    std::lock_guard<std::mutex> lock(impl_->tracker_mutex);
    impl_->learned_keys.clear();
    impl_->learned_ready.store(false);
    impl_->learn_mode.store(true);
}

bool WindowsHotkeyListener::TryTakeLearnedBinding(std::string& out) {
    if (!impl_->learned_ready.load()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->tracker_mutex);
    if (!impl_->learned_ready.load()) {
        return false;
    }
    out = impl_->learned_binding;
    impl_->learned_ready.store(false);
    return true;
}

bool WindowsHotkeyListener::HookInstalled() const {
    return impl_->hook_installed.load();
}
