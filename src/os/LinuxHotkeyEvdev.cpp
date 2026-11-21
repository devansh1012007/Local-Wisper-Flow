#include "os/LinuxHotkeyEvdev.h"

#include "core/Log.h"
#include "os/LinuxKeysym.h"

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <deque>
#include <set>
#include <stdexcept>

// Frozen kernel ABI (linux/input.h). Declared locally instead of including
// the kernel header so the constants are pinned and visible in review.
namespace {

struct InputEvent {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
    std::uint16_t type;
    std::uint16_t code;
    std::int32_t value;
};

constexpr std::uint16_t kEvSyn = 0x00;
constexpr std::uint16_t kEvKey = 0x01;

constexpr int kMaxDevices = 8;          // keyboards rarely exceed a handful
constexpr int kPollTimeoutMs = 100;

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::vector<std::string> ListEventDevices() {
    std::vector<std::string> devices;
    DIR* dir = ::opendir("/dev/input");
    if (dir == nullptr) {
        return devices;
    }
    while (const dirent* entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.rfind("event", 0) == 0) {
            devices.push_back("/dev/input/" + name);
        }
    }
    ::closedir(dir);
    std::sort(devices.begin(), devices.end());
    return devices;
}

}  // namespace

struct LinuxHotkeyEvdev::Impl {
    std::mutex state_mutex;  // guards bindings_, tracker_, pressed_
    std::vector<Binding> bindings;
    HotkeyTracker tracker;
    std::set<std::uint32_t> pressed;  // worker-side mirror of down keys

    std::mutex queue_mutex;
    std::deque<EngineEvent> events;

    std::atomic<bool> stop{false};
    std::atomic<bool> devices_open{false};

    std::vector<int> fds;
    std::thread worker;
};

LinuxHotkeyEvdev::LinuxHotkeyEvdev(std::vector<Binding> bindings,
                                   HotkeyTracker::Options options)
    : impl_(new Impl) {
    impl_->bindings = std::move(bindings);
    impl_->tracker.SetOptions(options);
}

LinuxHotkeyEvdev::~LinuxHotkeyEvdev() {
    Stop();
}

bool LinuxHotkeyEvdev::DevicesAvailable() {
    for (const std::string& device : ListEventDevices()) {
        if (::access(device.c_str(), R_OK) == 0) {
            return true;
        }
    }
    return false;
}

void LinuxHotkeyEvdev::Start() {
    if (!impl_->fds.empty()) {
        return;  // already running
    }
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        impl_->tracker.SetBindings(impl_->bindings);
    }
    for (const std::string& device : ListEventDevices()) {
        if (impl_->fds.size() >= static_cast<std::size_t>(kMaxDevices)) {
            break;
        }
        const int fd = ::open(device.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            impl_->fds.push_back(fd);
            lwf::Log::Info("evdev hotkey: listening on " + device);
        }
    }
    if (impl_->fds.empty()) {
        throw std::runtime_error(
            "no readable /dev/input/event* devices (Wayland hotkey needs the "
            "`input` group: sudo usermod -aG input $USER, then re-login)");
    }
    impl_->devices_open.store(true);
    impl_->stop.store(false);
    impl_->worker = std::thread([this] { RunLoop(); });
}

void LinuxHotkeyEvdev::Stop() {
    if (impl_->fds.empty()) {
        return;
    }
    impl_->stop.store(true);
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    for (const int fd : impl_->fds) {
        ::close(fd);
    }
    impl_->fds.clear();
    impl_->devices_open.store(false);
}

bool LinuxHotkeyEvdev::PollEvent(EngineEvent& out) {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    if (impl_->events.empty()) {
        return false;
    }
    out = impl_->events.front();
    impl_->events.pop_front();
    return true;
}

void LinuxHotkeyEvdev::SetCancelArmed(bool armed) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->tracker.SetCancelArmed(armed);
}

void LinuxHotkeyEvdev::UpdateBindings(std::vector<Binding> bindings) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->bindings = std::move(bindings);
    impl_->tracker.SetBindings(impl_->bindings);
}

void LinuxHotkeyEvdev::SetTrackerOptions(const HotkeyTracker::Options& options) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->tracker.SetOptions(options);
}

void LinuxHotkeyEvdev::RunLoop() {
    std::vector<pollfd> poll_fds;
    poll_fds.reserve(impl_->fds.size());
    for (const int fd : impl_->fds) {
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        poll_fds.push_back(pfd);
    }

    while (!impl_->stop.load()) {
        const int ready = ::poll(poll_fds.data(), static_cast<nfds_t>(poll_fds.size()),
                                 kPollTimeoutMs);
        if (ready <= 0) {
            if (ready == 0) {
                std::vector<EngineEvent> ticked;
                {
                    std::lock_guard<std::mutex> lock(impl_->state_mutex);
                    ticked = impl_->tracker.Tick(NowMs()).events;
                }
                PushEvents(ticked);
            }
            continue;
        }

        for (struct pollfd& pfd : poll_fds) {
            if ((pfd.revents & POLLIN) == 0) {
                continue;
            }
            // Drain the device; sync events flush the batch.
            for (;;) {
                InputEvent event{};
                const ssize_t got = ::read(pfd.fd, &event, sizeof(event));
                if (got != static_cast<ssize_t>(sizeof(event))) {
                    break;  // EAGAIN (drained), EINTR or device gone
                }
                if (event.type != kEvKey) {
                    continue;
                }
                if (event.value != 0 && event.value != 1) {
                    continue;  // 2 = autorepeat
                }
                const std::uint32_t keysym = lwf::linuxkeys::EvdevCodeToKeysym(event.code);
                const std::uint32_t vk = lwf::linuxkeys::KeysymToCoreVk(keysym);
                if (vk == 0) {
                    continue;
                }

                RawKeyEvent raw;
                raw.vk = vk;
                raw.is_down = event.value == 1;
                raw.injected = false;
                raw.timestamp_ms = NowMs();

                TrackerResult result;
                {
                    std::lock_guard<std::mutex> lock(impl_->state_mutex);
                    if (raw.is_down) {
                        impl_->pressed.insert(vk);
                    } else {
                        impl_->pressed.erase(vk);
                    }
                    result = impl_->tracker.Feed(raw);
                }
                PushEvents(result.events);
            }
        }
    }
}

void LinuxHotkeyEvdev::PushEvents(const std::vector<EngineEvent>& events) {
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
