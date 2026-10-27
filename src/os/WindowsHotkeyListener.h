#pragma once

#include "core/IHotkeyListener.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

class WindowsHotkeyListener : public IHotkeyListener {
public:
    static constexpr std::uint32_t kRightAlt = 0xA5;
    static constexpr std::uint32_t kLeftCtrl = 0xA2;

    explicit WindowsHotkeyListener(std::uint32_t virtual_key = kRightAlt,
                                   bool avoid_altgr = true);
    ~WindowsHotkeyListener() override;

    WindowsHotkeyListener(const WindowsHotkeyListener&) = delete;
    WindowsHotkeyListener& operator=(const WindowsHotkeyListener&) = delete;

    void StartListening(std::function<void(bool is_pressed)> callback) override;

    void SetSwallowEnabled(bool enabled);

    bool HookInstalled() const;

private:
    void ThreadMain();

    std::function<void(bool is_pressed)> callback_;
    std::uint32_t virtual_key_;
    bool avoid_altgr_;
    std::thread thread_;
    std::atomic<std::uint32_t> thread_id_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> hook_installed_{false};
    std::atomic<bool> swallow_enabled_{true};
};
