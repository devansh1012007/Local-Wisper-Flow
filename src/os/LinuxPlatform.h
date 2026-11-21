#pragma once

// Platform glue for the Linux port (plan 2026-11-21, changes C02-C07).
//
// Session/backend detection, adapter factories with compile-time capability
// probing (X11, XTest, ncurses are optional at build time and degrade at
// runtime), and the daemon control surface the TUI uses (pid file, SIGHUP
// reload). Everything here is Linux-only; Windows sources never see it.

#include "core/HotkeyBinding.h"
#include "core/HotkeyTracker.h"
#include "core/SettingsStore.h"

#include <memory>
#include <string>
#include <vector>

class IHotkeyListener;
class ITextInjector;
class LinuxNotifier;
class LinuxTextInjector;

namespace lwf::linuxplatform {

enum class Session { X11, Wayland, Headless };

Session DetectSession();
const char* ToString(Session session);

// Capability report for logs, the TUI status page and tests.
struct Capabilities {
    bool x11_hotkey = false;   // built with X11 and a display is reachable
    bool evdev_hotkey = false; // /dev/input readable (input group)
    bool xtest_inject = false; // built with Xtst
    bool tui = false;          // built with ncurses
    bool toasts = false;       // notify-send on PATH
};
Capabilities ProbeCapabilities();

// Chooses and constructs the hotkey listener for this session and config:
// X11 grab first, evdev when Wayland + opted in. Returns nullptr (with a
// logged reason) when no backend can run; the daemon stays up without a
// hotkey instead of dying.
std::unique_ptr<IHotkeyListener> CreateHotkeyListener(const SettingsStore& settings);

// The injector, always available (clipboard path needs no build flags).
std::unique_ptr<LinuxTextInjector> CreateTextInjector();

// Daemon control used by the TUI: the flock'd lock file under
// XDG_RUNTIME_DIR (or /tmp) carries the daemon pid; SIGHUP is the reload
// handshake.
std::string DaemonLockFile();
int ReadDaemonPid();              // 0 when no daemon is running
bool IsProcessAlive(int pid);     // false for 0/negative/dead pids
bool SendReloadToDaemon(int pid); // SIGHUP; false when it failed

// Loads bindings exactly like the Windows path, plus the Linux-only
// hotkey_keysym single-key binding (plan C09).
std::vector<Binding> LoadBindingsLinux(const SettingsStore& settings);

}  // namespace lwf::linuxplatform
