#pragma once

// Global hotkey listener for X11 (plan 2026-11-21, change C03).
//
// Grabs each configured chord on the root window with XGrabKey (the X11
// equivalent of the Windows keyboard hook): events for the grabbed
// combinations are delivered to this app and swallowed from the focused
// application, press and release alike. Key state comes from the event's
// modifier mask, so chords like Ctrl+D work even though only the final key
// is grabbed. Everything is funneled through the portable HotkeyTracker,
// exactly like the Windows adapter, so double-tap hands-free, cancel-armed
// and the AltGr guard behave identically.

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

class LinuxHotkeyListener final : public IHotkeyListener {
public:
    LinuxHotkeyListener(std::vector<Binding> bindings, HotkeyTracker::Options options);
    ~LinuxHotkeyListener() override;

    LinuxHotkeyListener(const LinuxHotkeyListener&) = delete;
    LinuxHotkeyListener& operator=(const LinuxHotkeyListener&) = delete;

    void Start() override;
    void Stop() override;
    bool PollEvent(EngineEvent& out) override;
    void SetCancelArmed(bool armed) override;

    // Live rebinding (TUI saves / SIGHUP reload) and hotkey learning, mirroring
    // the Windows adapter surface.
    void UpdateBindings(std::vector<Binding> bindings);
    void SetTrackerOptions(const HotkeyTracker::Options& options);
    void BeginLearnMode();
    bool TryTakeLearnedBinding(std::string& out);
    bool HookInstalled() const;

    // True when an X server is reachable (DISPLAY set and connects).
    static bool DisplayAvailable();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void RunLoop();
    void HandlePendingEvents(void* display);
    void HandleLearnEvent(void* display, const void* event, std::uint32_t keysym);
    void PushEvents(const std::vector<EngineEvent>& events);
};
