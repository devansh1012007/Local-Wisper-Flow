#pragma once

#include "core/EngineTypes.h"
#include "core/HotkeyBinding.h"
#include "core/HotkeyTracker.h"
#include "core/IHotkeyListener.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class WindowsHotkeyListener : public IHotkeyListener {
public:
    WindowsHotkeyListener(std::vector<Binding> bindings, HotkeyTracker::Options options);
    ~WindowsHotkeyListener() override;

    WindowsHotkeyListener(const WindowsHotkeyListener&) = delete;
    WindowsHotkeyListener& operator=(const WindowsHotkeyListener&) = delete;

    void Start() override;
    void Stop() override;
    bool PollEvent(EngineEvent& out) override;
    void SetCancelArmed(bool armed) override;

    void UpdateBindings(std::vector<Binding> bindings);
    void SetTrackerOptions(const HotkeyTracker::Options& options);
    void BeginLearnMode(const std::string& target_key = "hotkey_ptt");
    bool TryTakeLearnedBinding(std::string& out);
    bool TryTakeLearnedBinding(std::string& out_chord, std::string& out_target);
    std::string PendingLearnTarget() const;
    bool HookInstalled() const;

public:
    struct Impl;
    static Impl* instance();

private:
    std::unique_ptr<Impl> impl_;
};
