#include "os/LinuxTextInjector.h"

#include "core/InjectionException.h"
#include "core/Log.h"
#include "os/LinuxNotifier.h"

#include <cstdlib>
#include <memory>
#include <thread>
#include <unistd.h>

#ifdef LWF_HAVE_XTST
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#endif

#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

constexpr int kToolTimeoutMs = 2000;

std::uint32_t DecodeUtf8(const std::string& text, std::size_t& index) {
    const unsigned char first = static_cast<unsigned char>(text[index]);
    std::size_t length = 1;
    if ((first & 0x80u) == 0x00u) {
        length = 1;
    } else if ((first & 0xE0u) == 0xC0u) {
        length = 2;
    } else if ((first & 0xF0u) == 0xE0u) {
        length = 3;
    } else if ((first & 0xF8u) == 0xF0u) {
        length = 4;
    }
    if (index + length > text.size()) {
        ++index;
        return 0xFFFFFFFFu;  // malformed tail: untypable, forces fallback
    }
    std::uint32_t codepoint = first & (length == 1 ? 0x7Fu : (0xFFu >> (length + 1)));
    for (std::size_t offset = 1; offset < length; ++offset) {
        const unsigned char next = static_cast<unsigned char>(text[index + offset]);
        if ((next & 0xC0u) != 0x80u) {
            index += length;
            return 0xFFFFFFFFu;
        }
        codepoint = (codepoint << 6) | (next & 0x3Fu);
    }
    index += length;
    return codepoint;
}

}  // namespace

struct LinuxTextInjector::Impl {
#ifdef LWF_HAVE_XTST
    Display* display = nullptr;

    ~Impl() {
        if (display != nullptr) {
            XCloseDisplay(display);
            display = nullptr;
        }
    }

    Display* Open() {
        if (display == nullptr) {
            display = XOpenDisplay(nullptr);
        }
        return display;
    }

    void TapKey(KeyCode keycode) {
        XTestFakeKeyEvent(display, keycode, True, CurrentTime);
        XTestFakeKeyEvent(display, keycode, False, CurrentTime);
    }

    void TapChord(KeyCode keycode, unsigned int mods) {
        const KeyCode shift = XKeysymToKeycode(display, XK_Shift_L);
        if ((mods & ShiftMask) != 0 && shift != 0) {
            XTestFakeKeyEvent(display, shift, True, CurrentTime);
        }
        TapKey(keycode);
        if ((mods & ShiftMask) != 0 && shift != 0) {
            XTestFakeKeyEvent(display, shift, False, CurrentTime);
        }
    }
#endif
};

LinuxTextInjector::LinuxTextInjector()
    : impl_(new Impl), runner_(lwf::process::RealRunner()) {}

LinuxTextInjector::~LinuxTextInjector() {
    delete impl_;
}

void LinuxTextInjector::SetRunner(lwf::process::Runner runner) {
    runner_ = std::move(runner);
}

LinuxTextInjector::Session LinuxTextInjector::DetectSession() {
    const char* forced = std::getenv("LWF_INJECT_MODE");
    if (forced != nullptr) {
        const std::string mode(forced);
        if (mode == "x11") {
            return Session::X11;
        }
        if (mode == "wayland" || mode == "clipboard") {
            return Session::Wayland;
        }
    }
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        return Session::Wayland;
    }
    if (std::getenv("DISPLAY") != nullptr) {
        return Session::X11;
    }
    return Session::Headless;
}

std::uint32_t LinuxTextInjector::CharToKeysym(std::uint32_t codepoint) {
    switch (codepoint) {
        case U'\n':
        case U'\r':
            return 0xff0d;  // XK_Return
        case U'\t':
            return 0xff09;  // XK_Tab
        default:
            break;
    }
    if (codepoint < 0x20u || codepoint == 0x7Fu) {
        return 0;  // other control characters: not typable
    }
    if (codepoint <= 0xFFu) {
        return codepoint;  // Latin-1 keysyms are the codepoints themselves
    }
    return 0x01000000u | codepoint;  // Unicode keysym range
}

std::string LinuxTextInjector::ChooseClipboardTool(Session session) {
    const char* wayland_tools[] = {"wl-copy", "xclip", "xsel"};
    const char* x11_tools[] = {"xclip", "xsel", "wl-copy"};
    const char* const* tools = session == Session::Wayland ? wayland_tools : x11_tools;
    const int count = session == Session::Wayland
                          ? static_cast<int>(sizeof(wayland_tools) / sizeof(wayland_tools[0]))
                          : static_cast<int>(sizeof(x11_tools) / sizeof(x11_tools[0]));
    for (int index = 0; index < count; ++index) {
        if (lwf::process::Exists(tools[index])) {
            return tools[index];
        }
    }
    return std::string();
}

bool LinuxTextInjector::InjectViaTyping(const std::string& text) {
#ifdef LWF_HAVE_XTST
    Display* display = impl_->Open();
    if (display == nullptr) {
        return false;
    }
    std::vector<std::pair<KeyCode, unsigned int>> plan;
    std::size_t index = 0;
    while (index < text.size()) {
        const std::uint32_t codepoint = DecodeUtf8(text, index);
        const std::uint32_t keysym = CharToKeysym(codepoint);
        if (keysym == 0) {
            return false;  // control characters: fall back to clipboard
        }
        // Classic keysym -> (keycode, shift) resolution: the keycode via
        // XKeysymToKeycode, then compare the key's level-0/level-1 keysyms
        // to decide whether Shift is required. Anything reachable only
        // through AltGr or dead levels falls back to the clipboard path.
        const KeyCode keycode = XKeysymToKeycode(display, keysym);
        if (keycode == 0) {
            return false;  // not on this keyboard layout
        }
        unsigned int mods = 0;
        const KeySym level0 = XkbKeycodeToKeysym(display, keycode, 0, 0);
        const KeySym level1 = XkbKeycodeToKeysym(display, keycode, 0, 1);
        if (level0 != keysym) {
            if (level1 == keysym) {
                mods |= ShiftMask;
            } else {
                return false;  // AltGr/dead-level only: too layout-fragile
            }
        }
        plan.emplace_back(keycode, mods);
    }

    unsigned int sync_counter = 0;
    for (const auto& step : plan) {
        impl_->TapChord(step.first, step.second);
        if (++sync_counter % 16 == 0) {
            XSync(display, False);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    }
    XSync(display, False);
    return true;
#else
    (void)text;
    return false;  // built without XTest: clipboard path only
#endif
}

void LinuxTextInjector::InjectViaClipboard(const std::string& text) {
    const Session session = DetectSession();
    const std::string tool = ChooseClipboardTool(session);
    if (tool.empty()) {
        throw InjectionException(
            ErrorCode::InjectFailed,
            "no clipboard helper found (install wl-clipboard or xclip)");
    }

    lwf::process::Command readback;
    std::string previous;
    bool had_previous = false;
    if (tool == "wl-copy") {
        readback.executable = session == Session::Wayland ? "wl-paste" : "xclip";
    } else {
        readback.executable = tool;
    }
    if (readback.executable == "wl-paste") {
        readback.args = {"--no-newline"};
    } else if (readback.executable == "xclip") {
        readback.args = {"-selection", "clipboard", "-o"};
    } else if (readback.executable == "xsel") {
        readback.args = {"--clipboard", "--output"};
    }
    if (!readback.executable.empty() && lwf::process::Exists(readback.executable)) {
        const lwf::process::RunResult saved = runner_(readback, "", kToolTimeoutMs);
        if (saved.launched && saved.exit_code == 0) {
            previous = saved.output;
            had_previous = true;
        }
    }

    lwf::process::Command copy;
    copy.executable = tool;
    if (tool == "wl-copy") {
        copy.args = {"--type", "text/plain;charset=utf-8"};
    } else if (tool == "xclip") {
        copy.args = {"-selection", "clipboard"};
    } else if (tool == "xsel") {
        copy.args = {"--clipboard", "--input"};
    }
    const lwf::process::RunResult copied = runner_(copy, text, kToolTimeoutMs);
    if (!copied.launched || copied.exit_code != 0) {
        throw InjectionException(ErrorCode::ClipboardBusy,
                                 "clipboard copy failed: " +
                                     (copied.error.empty()
                                          ? ("exit " + std::to_string(copied.exit_code))
                                          : copied.error));
    }

    if (inject_delay_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(inject_delay_ms_));
    }

#ifdef LWF_HAVE_XTST
    if (session == Session::X11 && PasteViaXTest()) {
        if (had_previous) {
            RestoreClipboardLater(previous);
        }
        return;
    }
#else
    (void)0;
#endif
    // Wayland (or X11 without XTest): the user pastes manually.
    if (notifier_ != nullptr) {
        notifier_->Notify("Dictation ready", "Press Ctrl+V to paste", 5000);
    }
    if (had_previous) {
        RestoreClipboardLater(previous);
    }
}

bool LinuxTextInjector::PasteViaXTest() {
#ifdef LWF_HAVE_XTST
    Display* display = impl_->Open();
    if (display == nullptr) {
        return false;
    }
    const KeyCode control = XKeysymToKeycode(display, XK_Control_L);
    const KeyCode v = XKeysymToKeycode(display, XK_v);
    if (control == 0 || v == 0) {
        return false;
    }
    XTestFakeKeyEvent(display, control, True, CurrentTime);
    impl_->TapKey(v);
    XTestFakeKeyEvent(display, control, False, CurrentTime);
    XSync(display, False);
    return true;
#else
    return false;
#endif
}

void LinuxTextInjector::RestoreClipboardLater(const std::string& saved) {
    // Best-effort mirror of the Windows restore: give the target app a beat
    // to consume the paste, then put the previous content back if the
    // clipboard still holds our text. Runs detached; the runner is copied by
    // value, which is safe because the real runner is stateless.
    lwf::process::Runner runner = runner_;
    std::thread([runner, saved]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const Session session = LinuxTextInjector::DetectSession();
        lwf::process::Command copy;
        if (session == Session::Wayland && lwf::process::Exists("wl-copy")) {
            copy.executable = "wl-copy";
            copy.args = {"--type", "text/plain;charset=utf-8"};
        } else if (lwf::process::Exists("xclip")) {
            copy.executable = "xclip";
            copy.args = {"-selection", "clipboard"};
        } else if (lwf::process::Exists("xsel")) {
            copy.executable = "xsel";
            copy.args = {"--clipboard", "--input"};
        } else {
            return;
        }
        runner(copy, saved, kToolTimeoutMs);
    }).detach();
}

void LinuxTextInjector::InjectText(const std::string& text) {
    if (text.empty()) {
        return;
    }
    const Session session = DetectSession();
    if (session == Session::Headless) {
        throw InjectionException(ErrorCode::InjectFailed,
                                 "no display server; cannot place text in an app");
    }

#ifdef LWF_HAVE_XTST
    if (session == Session::X11 && InjectViaTyping(text)) {
        return;
    }
    lwf::Log::Debug(session == Session::X11
                        ? "direct typing unavailable; using clipboard path"
                        : "wayland session; using clipboard path");
#else
    lwf::Log::Debug("built without XTest; using clipboard path");
#endif
    InjectViaClipboard(text);
}
