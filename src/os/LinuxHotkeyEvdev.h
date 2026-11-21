#pragma once

// Opt-in global hotkey listener for Wayland sessions (plan 2026-11-21,
// change C04, decision D3).
//
// Wayland has no global-shortcuts portal on stock Ubuntu, so the opt-in
// backend reads keyboard events straight from /dev/input/event* — which the
// kernel only allows for the `input` group (documented in
// docs/UBUNTU_DEPS.md; `sudo usermod -aG input $USER` then re-login).
// Events are read passively (no EVIOCGRAB): the desktop keeps receiving
// them, exactly like the X11 grab does not disturb other clients. Only the
// configured chords are consumed; everything else is ignored.
//
// This is keylogger-adjacent by construction (any reader of /dev/input sees
// all keys), which is why it ships disabled (hotkey_evdev_enabled=false) and
// the docs call the privilege out loudly.

#include "core/EngineTypes.h"
#include "core/HotkeyBinding.h"
#include "core/HotkeyTracker.h"
#include "core/IHotkeyListener.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class LinuxHotkeyEvdev final : public IHotkeyListener {
public:
    LinuxHotkeyEvdev(std::vector<Binding> bindings, HotkeyTracker::Options options);
    ~LinuxHotkeyEvdev() override;

    LinuxHotkeyEvdev(const LinuxHotkeyEvdev&) = delete;
    LinuxHotkeyEvdev& operator=(const LinuxHotkeyEvdev&) = delete;

    void Start() override;
    void Stop() override;
    bool PollEvent(EngineEvent& out) override;
    void SetCancelArmed(bool armed) override;

    void UpdateBindings(std::vector<Binding> bindings);
    void SetTrackerOptions(const HotkeyTracker::Options& options);

    // True when at least one /dev/input/event* device is readable.
    static bool DevicesAvailable();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void RunLoop();
    void PushEvents(const std::vector<EngineEvent>& events);
};
