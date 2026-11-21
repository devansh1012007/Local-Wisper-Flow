#pragma once

// Text injection for the Linux port (plan 2026-11-21, change C05, decision
// D4). Two paths, chosen by session type:
//
//   X11     - direct typing through the XTest extension (XSendKey-style
//             XTestFakeKeyEvent), which lands in the focused window; if a
//             character has no keycode on the current layout the whole text
//             falls back to the clipboard path.
//   Wayland - no injection API exists for unfocused apps: the text is placed
//             on the clipboard (wl-copy, xclip, xsel — first available) and
//             a notify-send toast asks the user to press Ctrl+V.
//
// On X11 the clipboard path additionally synthesizes Ctrl+V via XTest, so
// the user-visible result is the same as direct typing. Like the Windows
// adapter, the previous clipboard content is restored best-effort shortly
// after the paste.

#include "core/ITextInjector.h"
#include "os/LinuxProcess.h"

#include <cstdint>
#include <functional>
#include <string>

class LinuxNotifier;

class LinuxTextInjector final : public ITextInjector {
public:
    enum class Session { X11, Wayland, Headless };

    LinuxTextInjector();
    ~LinuxTextInjector() override;

    LinuxTextInjector(const LinuxTextInjector&) = delete;
    LinuxTextInjector& operator=(const LinuxTextInjector&) = delete;

    void InjectText(const std::string& text) override;

    void SetInjectDelayMs(int delay_ms) { inject_delay_ms_ = delay_ms; }
    void SetNotifier(LinuxNotifier* notifier) { notifier_ = notifier; }

    // Subprocess indirection for tests; defaults to the real runner.
    void SetRunner(lwf::process::Runner runner);

    // Session detection honoring the LWF_INJECT_MODE override (x11/wayland/
    // clipboard) so behavior is testable and users can force a path.
    static Session DetectSession();

    // Keysym for one Unicode codepoint as used by the typing path: control
    // characters map to their X keysyms (Return, Tab), Latin-1 passes
    // through, everything else becomes the 0x01000000|cp keysym.
    static std::uint32_t CharToKeysym(std::uint32_t codepoint);

    // First available clipboard helper for the session, or nullptr-name.
    static std::string ChooseClipboardTool(Session session);

private:
    bool InjectViaTyping(const std::string& text);
    void InjectViaClipboard(const std::string& text);
    bool PasteViaXTest();
    void RestoreClipboardLater(const std::string& saved);

    struct Impl;
    Impl* impl_;  // X11 state; opaque so this header stays X11-free
    LinuxNotifier* notifier_ = nullptr;
    lwf::process::Runner runner_;
    int inject_delay_ms_ = 0;
};
