#include "os/LinuxPlatform.h"

#include "core/Log.h"
#include "os/LinuxHotkeyEvdev.h"
#include "os/LinuxHotkeyListener.h"
#include "os/LinuxKeysym.h"
#include "os/LinuxNotifier.h"
#include "os/LinuxTextInjector.h"

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace lwf::linuxplatform {
namespace {

bool X11BuildEnabled() {
#if defined(LWF_HAVE_X11)
    return true;
#else
    return false;
#endif
}

bool XtstBuildEnabled() {
#if defined(LWF_HAVE_XTST)
    return true;
#else
    return false;
#endif
}

bool TuiBuildEnabled() {
#if defined(LWF_HAVE_CURSES)
    return true;
#else
    return false;
#endif
}

std::string RuntimeDir() {
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg != nullptr && *xdg != '\0') {
        return std::string(xdg);
    }
    return "/tmp";
}

}  // namespace

Session DetectSession() {
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
        return Session::Wayland;
    }
    if (std::getenv("DISPLAY") != nullptr) {
        return Session::X11;
    }
    return Session::Headless;
}

const char* ToString(Session session) {
    switch (session) {
        case Session::X11:
            return "x11";
        case Session::Wayland:
            return "wayland";
        case Session::Headless:
            return "headless";
    }
    return "unknown";
}

Capabilities ProbeCapabilities() {
    Capabilities caps;
    caps.x11_hotkey = X11BuildEnabled() && LinuxHotkeyListener::DisplayAvailable();
    caps.evdev_hotkey = LinuxHotkeyEvdev::DevicesAvailable();
    caps.xtest_inject = XtstBuildEnabled();
    caps.tui = TuiBuildEnabled();
    caps.toasts = LinuxNotifier().Available();
    return caps;
}

std::unique_ptr<IHotkeyListener> CreateHotkeyListener(const SettingsStore& settings) {
    HotkeyTracker::Options options;
    options.avoid_altgr_conflict = settings.GetBool("avoid_altgr_conflict");
    options.double_tap_window_ms = settings.GetInt("double_tap_window_ms");
    std::vector<Binding> bindings = LoadBindingsLinux(settings);

    const Session session = DetectSession();
    if (session == Session::X11 && X11BuildEnabled()) {
        try {
            auto listener = std::make_unique<LinuxHotkeyListener>(bindings, options);
            listener->Start();
            lwf::Log::Info("hotkey backend: X11 grabs");
            return listener;
        } catch (const std::exception& e) {
            lwf::Log::Error(std::string("X11 hotkey failed to start: ") + e.what());
            LinuxNotifier().Notify("Hotkey error", e.what(), 6000, LinuxNotifier::Urgency::Normal);
            // Fall through: evdev is still worth trying (XWayland grabs can
            // fail while /dev/input still works).
        }
    }
    if (settings.GetBool("hotkey_evdev_enabled")) {
        try {
            auto listener = std::make_unique<LinuxHotkeyEvdev>(bindings, options);
            listener->Start();
            lwf::Log::Info("hotkey backend: evdev (opt-in)");
            return listener;
        } catch (const std::exception& e) {
            lwf::Log::Error(std::string("evdev hotkey failed to start: ") + e.what());
            LinuxNotifier().Notify("Hotkey error", e.what(), 6000, LinuxNotifier::Urgency::Normal);
        }
    } else if (session == Session::Wayland) {
        lwf::Log::Warning(
            "Wayland session without hotkey_evdev_enabled: no global hotkey; "
            "enable it in the TUI (needs the `input` group, see docs/UBUNTU_DEPS.md)");
    } else if (!X11BuildEnabled()) {
        lwf::Log::Warning("built without X11: no global hotkey backend available");
    }
    return nullptr;
}

std::unique_ptr<LinuxTextInjector> CreateTextInjector() {
    auto injector = std::make_unique<LinuxTextInjector>();
    return injector;
}

std::vector<Binding> LoadBindingsLinux(const SettingsStore& settings) {
    std::vector<Binding> bindings = LoadBindings(settings);
    // Additive Linux key (plan C09): a single-key push-to-talk keysym that
    // exists only on Linux; 0 (default) means unmapped. The keysym is
    // translated to the core VK code so both backends match it identically.
    const int keysym = settings.GetInt("hotkey_keysym");
    if (keysym > 0) {
        const std::uint32_t vk = linuxkeys::KeysymToCoreVk(static_cast<std::uint32_t>(keysym));
        if (vk != 0) {
            Binding binding;
            binding.role = BindingRole::PushToTalk;
            binding.keys.push_back(vk);
            binding.label = linuxkeys::KeysymDisplayName(static_cast<std::uint32_t>(keysym));
            bindings.push_back(std::move(binding));
        } else {
            lwf::Log::Warning("hotkey_keysym " + std::to_string(keysym) +
                              " has no core key mapping; ignored");
        }
    }
    return bindings;
}

std::string DaemonLockFile() {
    return (std::filesystem::path(RuntimeDir()) / "localwisperflow-daemon.lock").string();
}

int ReadDaemonPid() {
    std::ifstream file(DaemonLockFile());
    int pid = 0;
    if (file >> pid && pid > 0 && IsProcessAlive(pid)) {
        return pid;
    }
    return 0;
}

bool IsProcessAlive(int pid) {
    if (pid <= 0) {
        return false;
    }
    return ::kill(pid, 0) == 0;
}

bool SendReloadToDaemon(int pid) {
    if (pid <= 0) {
        return false;
    }
    return ::kill(pid, SIGHUP) == 0;
}

}  // namespace lwf::linuxplatform
