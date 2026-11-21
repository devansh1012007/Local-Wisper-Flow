#include "os/LinuxHotkeyListener.h"

#include "core/Log.h"
#include "os/LinuxKeysym.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <poll.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <set>
#include <stdexcept>

namespace {

using lwf::linuxkeys::CoreVkToKeysym;
using lwf::linuxkeys::KeysymToCoreVk;

// Grab each chord under every lock-modifier combination, so NumLock or
// CapsLock being on does not silently disable the hotkey.
constexpr unsigned int kLockMasks[] = {
    0,
    LockMask,
    Mod2Mask,
    LockMask | Mod2Mask,
};

struct X11ErrorState {
    bool bad_access = false;  // another client already grabbed the key
    bool bad_value = false;   // keysym has no keycode on this layout
};

X11ErrorState g_grab_errors;

int GrabErrorHandler(Display*, XErrorEvent* event) {
    if (event->error_code == BadAccess) {
        g_grab_errors.bad_access = true;
    } else {
        g_grab_errors.bad_value = true;
    }
    return 0;
}

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// The modifier set implied by an event's state mask. Mod1 carries no
// left/right information, so both the generic and the left code count as
// pressed: "Alt+X" chords match either physical Alt key.
void AddStateModifiers(unsigned int state, std::set<std::uint32_t>& pressed) {
    if ((state & ControlMask) != 0) {
        pressed.insert(lwf::linuxkeys::kVkCtrl);
    }
    if ((state & ShiftMask) != 0) {
        pressed.insert(lwf::linuxkeys::kVkShift);
    }
    if ((state & Mod1Mask) != 0) {
        pressed.insert(lwf::linuxkeys::kVkAlt);
        pressed.insert(lwf::linuxkeys::kVkLeftAlt);
    }
    if ((state & Mod4Mask) != 0) {
        pressed.insert(lwf::linuxkeys::kVkWin);
    }
}

}  // namespace

struct HotkeyGrab {
    KeyCode keycode = 0;
    unsigned int modifiers = 0;
};

struct LinuxHotkeyListener::Impl {

    std::mutex state_mutex;  // guards bindings_, tracker_
    std::vector<Binding> bindings;
    HotkeyTracker tracker;

    std::mutex queue_mutex;  // guards events_, learned binding
    std::deque<EngineEvent> events;
    std::string learned_binding;
    bool has_learned = false;

    std::vector<HotkeyGrab> grabs;  // worker-side, owned by the worker thread

    std::atomic<bool> stop{false};
    std::atomic<bool> hook_installed{false};
    std::atomic<bool> learn_requested{false};
    std::atomic<bool> regrab_requested{false};
    std::atomic<bool> worker_running{false};

    // Startup handshake: the worker owns the Xlib Display end to end (Xlib
    // Displays are thread-bound), and Start() blocks until the worker has
    // either installed the grabs or collected a fatal error.
    std::mutex start_mutex;
    std::condition_variable start_cv;
    bool startup_done = false;
    std::string start_error;

    bool learn_active = false;  // worker-side learn state

    Display* display = nullptr;  // worker-owned after Start()
    Window root = 0;
    std::thread worker;
};

namespace {

// The XGrabKey set for one binding list: the non-modifier key (if any)
// grabbed under the chord's modifier mask; modifiers-only chords grab each
// participating modifier under the mask formed by the others.
std::vector<HotkeyGrab> ComputeGrabs(Display* display, const std::vector<Binding>& bindings) {
    std::vector<HotkeyGrab> grabs;
    for (const Binding& binding : bindings) {
        unsigned int modmask = 0;
        std::vector<KeyCode> main_keys;
        std::vector<KeyCode> modifier_keys;
        for (const std::uint32_t vk : binding.keys) {
            const KeyCode keycode = XKeysymToKeycode(display, CoreVkToKeysym(vk));
            switch (vk) {
                case lwf::linuxkeys::kVkCtrl:
                    modmask |= ControlMask;
                    modifier_keys.push_back(keycode);
                    break;
                case lwf::linuxkeys::kVkShift:
                    modmask |= ShiftMask;
                    modifier_keys.push_back(keycode);
                    break;
                case lwf::linuxkeys::kVkAlt:
                case lwf::linuxkeys::kVkLeftAlt:
                case lwf::linuxkeys::kVkRightAlt:
                    modmask |= Mod1Mask;
                    modifier_keys.push_back(keycode);
                    break;
                case lwf::linuxkeys::kVkWin:
                    modmask |= Mod4Mask;
                    modifier_keys.push_back(keycode);
                    break;
                default:
                    main_keys.push_back(keycode);
                    break;
            }
        }
        const auto add_grab = [&](KeyCode keycode, unsigned int mask) {
            if (keycode == 0) {
                return;
            }
            for (const unsigned int lock : kLockMasks) {
                grabs.push_back(HotkeyGrab{keycode, mask | lock});
            }
        };
        if (!main_keys.empty()) {
            for (const KeyCode keycode : main_keys) {
                add_grab(keycode, modmask);
            }
        } else {
            // Modifiers-only chord (e.g. RightAlt alone): grab each modifier
            // keycode under the mask formed by the other participants.
            for (std::size_t i = 0; i < modifier_keys.size() && i < binding.keys.size(); ++i) {
                unsigned int others = modmask;
                switch (binding.keys[i]) {
                    case lwf::linuxkeys::kVkCtrl:
                        others = modmask & ~ControlMask;
                        break;
                    case lwf::linuxkeys::kVkShift:
                        others = modmask & ~ShiftMask;
                        break;
                    case lwf::linuxkeys::kVkAlt:
                    case lwf::linuxkeys::kVkLeftAlt:
                    case lwf::linuxkeys::kVkRightAlt:
                        others = modmask & ~Mod1Mask;
                        break;
                    case lwf::linuxkeys::kVkWin:
                        others = modmask & ~Mod4Mask;
                        break;
                    default:
                        break;
                }
                add_grab(modifier_keys[i], others);
            }
        }
    }
    return grabs;
}

}  // namespace

LinuxHotkeyListener::LinuxHotkeyListener(std::vector<Binding> bindings,
                                         HotkeyTracker::Options options)
    : impl_(new Impl) {
    impl_->bindings = std::move(bindings);
    impl_->tracker.SetOptions(options);
}

LinuxHotkeyListener::~LinuxHotkeyListener() {
    Stop();
}

bool LinuxHotkeyListener::DisplayAvailable() {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        return false;
    }
    XCloseDisplay(display);
    return true;
}

void LinuxHotkeyListener::Start() {
    if (impl_->worker_running.load()) {
        return;  // already running
    }

    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        impl_->tracker.SetBindings(impl_->bindings);
    }

    impl_->stop.store(false);
    {
        std::lock_guard<std::mutex> lock(impl_->start_mutex);
        impl_->startup_done = false;
        impl_->start_error.clear();
    }
    impl_->worker = std::thread([this] { RunLoop(); });

    // Fail fast on the worker's behalf: display unreachable or a chord
    // already owned by another application both surface as exceptions from
    // Start, exactly like the Windows hook adapter.
    std::unique_lock<std::mutex> lock(impl_->start_mutex);
    impl_->start_cv.wait(lock, [this] { return impl_->startup_done; });
    if (!impl_->start_error.empty()) {
        lock.unlock();
        if (impl_->worker.joinable()) {
            impl_->worker.join();
        }
        throw std::runtime_error(impl_->start_error);
    }
}

void LinuxHotkeyListener::Stop() {
    if (!impl_->worker_running.load()) {
        return;
    }
    impl_->stop.store(true);
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    impl_->hook_installed.store(false);
}

bool LinuxHotkeyListener::PollEvent(EngineEvent& out) {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    if (impl_->events.empty()) {
        return false;
    }
    out = impl_->events.front();
    impl_->events.pop_front();
    return true;
}

void LinuxHotkeyListener::SetCancelArmed(bool armed) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->tracker.SetCancelArmed(armed);
}

void LinuxHotkeyListener::UpdateBindings(std::vector<Binding> bindings) {
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        impl_->bindings = std::move(bindings);
        impl_->tracker.SetBindings(impl_->bindings);
    }
    if (impl_->worker_running.load()) {
        // The Xlib Display is worker-owned; the worker re-grabs on its next
        // loop pass.
        impl_->regrab_requested.store(true);
    }
}

void LinuxHotkeyListener::SetTrackerOptions(const HotkeyTracker::Options& options) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->tracker.SetOptions(options);
}

void LinuxHotkeyListener::BeginLearnMode() {
    impl_->learn_requested.store(true);
}

bool LinuxHotkeyListener::TryTakeLearnedBinding(std::string& out) {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    if (!impl_->has_learned) {
        return false;
    }
    impl_->has_learned = false;
    out = impl_->learned_binding;
    return true;
}

bool LinuxHotkeyListener::HookInstalled() const {
    return impl_ != nullptr && impl_->hook_installed.load();
}

void LinuxHotkeyListener::RunLoop() {
    impl_->worker_running.store(true);

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        {
            std::lock_guard<std::mutex> lock(impl_->start_mutex);
            impl_->start_error =
                "cannot open X display (DISPLAY not set or X server unreachable)";
            impl_->startup_done = true;
        }
        impl_->start_cv.notify_all();
        impl_->worker_running.store(false);
        return;
    }
    impl_->display = display;
    impl_->root = DefaultRootWindow(display);

    // Grab phase: a non-fatal error handler records BadAccess (key owned by
    // another client — the documented fail-fast case) and bad values
    // (keysym without keycode on this layout — chord stays inactive).
    g_grab_errors = X11ErrorState{};
    int (*previous_handler)(Display*, XErrorEvent*) = XSetErrorHandler(&GrabErrorHandler);
    const std::vector<HotkeyGrab> grabs = ComputeGrabs(display, impl_->bindings);
    for (const HotkeyGrab& grab : grabs) {
        XGrabKey(display, grab.keycode, grab.modifiers, impl_->root, False, GrabModeAsync,
                 GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(previous_handler);

    if (g_grab_errors.bad_access) {
        for (const HotkeyGrab& grab : grabs) {
            XUngrabKey(display, grab.keycode, grab.modifiers, impl_->root);
        }
        XSync(display, False);
        {
            std::lock_guard<std::mutex> lock(impl_->start_mutex);
            impl_->start_error =
                "hotkey chord already grabbed by another application (XGrabKey BadAccess)";
            impl_->startup_done = true;
        }
        impl_->start_cv.notify_all();
        XCloseDisplay(display);
        impl_->display = nullptr;
        impl_->worker_running.store(false);
        return;
    }
    if (g_grab_errors.bad_value) {
        lwf::Log::Warning("some hotkey keysyms have no keycode on this keyboard layout; "
                          "those chords are inactive");
    }
    bool any_grabbed = false;
    for (const HotkeyGrab& grab : grabs) {
        if (grab.keycode != 0) {
            any_grabbed = true;
        }
    }
    impl_->hook_installed.store(any_grabbed);
    impl_->grabs = grabs;
    lwf::Log::Info("x11 hotkey: " + std::to_string(grabs.size()) + " grabs installed");
    {
        std::lock_guard<std::mutex> lock(impl_->start_mutex);
        impl_->startup_done = true;
    }
    impl_->start_cv.notify_all();

    const int fd = XConnectionNumber(display);
    while (!impl_->stop.load()) {
        if (impl_->regrab_requested.exchange(false)) {
            std::vector<HotkeyGrab> grabs;
            {
                std::lock_guard<std::mutex> lock(impl_->state_mutex);
                grabs = ComputeGrabs(display, impl_->bindings);
            }
            for (const HotkeyGrab& grab : impl_->grabs) {
                XUngrabKey(display, grab.keycode, grab.modifiers, impl_->root);
            }
            g_grab_errors = X11ErrorState{};
            int (*previous)(Display*, XErrorEvent*) = XSetErrorHandler(&GrabErrorHandler);
            for (const HotkeyGrab& grab : grabs) {
                XGrabKey(display, grab.keycode, grab.modifiers, impl_->root, False,
                         GrabModeAsync, GrabModeAsync);
            }
            XSync(display, False);
            XSetErrorHandler(previous);
            impl_->grabs = grabs;
            if (g_grab_errors.bad_access) {
                lwf::Log::Error("hotkey re-grab hit BadAccess; a chord is owned by "
                                "another application and stays inactive");
            }
        }

        if (impl_->learn_requested.exchange(false)) {
            const int grab_result =
                XGrabKeyboard(display, impl_->root, False, GrabModeAsync, GrabModeAsync,
                              CurrentTime);
            if (grab_result == GrabSuccess) {
                impl_->learn_active = true;
                lwf::Log::Info("learn mode: press the new hotkey");
            } else {
                lwf::Log::Warning("learn mode: could not grab the keyboard");
            }
        }

        // Poll the X socket with a timeout so stop/regrab/learn flags are
        // honored even when no keys are pressed.
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int ready = ::poll(&pfd, 1, 100);
        if (ready > 0 && (pfd.revents & POLLIN) != 0) {
            HandlePendingEvents(display);
        }

        std::vector<EngineEvent> ticked;
        {
            std::lock_guard<std::mutex> lock(impl_->state_mutex);
            ticked = impl_->tracker.Tick(NowMs()).events;
        }
        PushEvents(ticked);
    }

    // Release the keyboard (learn mode may hold it) and every grab.
    XUngrabKeyboard(display, CurrentTime);
    for (const HotkeyGrab& grab : impl_->grabs) {
        XUngrabKey(display, grab.keycode, grab.modifiers, impl_->root);
    }
    XSync(display, False);
    XCloseDisplay(display);
    impl_->display = nullptr;
    impl_->worker_running.store(false);
}

void LinuxHotkeyListener::HandlePendingEvents(void* display_ptr) {
    Display* display = static_cast<Display*>(display_ptr);
    while (XPending(display) > 0) {
        XEvent event{};
        XNextEvent(display, &event);
        if (event.type != KeyPress && event.type != KeyRelease) {
            continue;
        }
        const std::uint32_t keysym = XLookupKeysym(&event.xkey, 0);
        const std::uint32_t vk = KeysymToCoreVk(keysym);

        if (impl_->learn_active) {
            HandleLearnEvent(display, &event, keysym);
            continue;
        }
        if (vk == 0) {
            continue;
        }

        RawKeyEvent raw;
        raw.vk = vk;
        raw.is_down = event.type == KeyPress;
        raw.injected = false;
        raw.timestamp_ms = NowMs();

        TrackerResult result;
        {
            std::lock_guard<std::mutex> lock(impl_->state_mutex);
            result = impl_->tracker.Feed(raw);
        }
        PushEvents(result.events);
    }
}

void LinuxHotkeyListener::HandleLearnEvent(void* display_ptr, const void* event_ptr,
                                           std::uint32_t keysym) {
    Display* display = static_cast<Display*>(display_ptr);
    const XEvent& event = *static_cast<const XEvent*>(event_ptr);
    if (event.type != KeyPress || lwf::linuxkeys::IsModifierKeysym(keysym)) {
        return;  // modifiers alone do not finish learning
    }
    impl_->learn_active = false;
    XUngrabKeyboard(display, CurrentTime);

    std::set<std::uint32_t> pressed;
    AddStateModifiers(event.xkey.state, pressed);
    std::vector<std::uint32_t> keys(pressed.begin(), pressed.end());
    const std::uint32_t vk = KeysymToCoreVk(keysym);
    if (vk != 0) {
        keys.push_back(vk);
    }
    Binding binding;
    binding.role = BindingRole::PushToTalk;
    binding.keys = keys;
    binding.label = HotkeyBinding::Serialize(binding);
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        impl_->learned_binding = binding.label;
        impl_->has_learned = true;
    }
    lwf::Log::Info("learn mode: captured " + binding.label);
}

void LinuxHotkeyListener::PushEvents(const std::vector<EngineEvent>& events) {
    if (events.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    for (const EngineEvent& event : events) {
        impl_->events.push_back(event);
        if (impl_->events.size() > 64) {
            impl_->events.pop_front();
        }
    }
}
