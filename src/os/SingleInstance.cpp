#include "os/SingleInstance.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

namespace {

#ifdef _WIN32

std::string SanitizedMutexName(std::string name) {
    for (char& c : name) {
        if (c == '\\' || c == '/') {
            c = '_';
        }
    }
    return "Local\\" + name;
}

#else

// POSIX daemon lock: an advisory flock on a lock file. The kernel drops the
// lock when the process dies, so a crashed daemon cannot block the next
// start, and a second daemon detects the running one immediately. The file
// also carries the pid for the TUI's SIGHUP reload targeting.
std::string LockPath(const std::string& mutex_name) {
    std::string sanitized = mutex_name;
    for (char& c : sanitized) {
        if (c == '\\' || c == '/') {
            c = '_';
        }
    }
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    const std::string dir =
        runtime != nullptr && *runtime != '\0' ? std::string(runtime) : std::string("/tmp");
    return dir + "/" + sanitized + ".lock";
}

#endif

}  // namespace

SingleInstance::SingleInstance(const std::string& mutex_name) {
#ifdef _WIN32
    const std::string name = SanitizedMutexName(mutex_name);
    handle_ = CreateMutexA(nullptr, TRUE, name.c_str());
    if (handle_ != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        ReleaseMutex(handle_);
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    const std::string lock_path = LockPath(mutex_name);
    const int fd = ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
    if (fd < 0) {
        handle_ = nullptr;
        return;
    }
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        handle_ = nullptr;
        return;
    }
    char pid_text[32];
    std::snprintf(pid_text, sizeof(pid_text), "%d\n", static_cast<int>(::getpid()));
    const std::size_t length = std::strlen(pid_text);
    const ssize_t ignored = ::write(fd, pid_text, length);
    (void)ignored;
    handle_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(fd));
#endif
}

SingleInstance::~SingleInstance() {
#ifdef _WIN32
    if (handle_ != nullptr) {
        ReleaseMutex(handle_);
        CloseHandle(handle_);
    }
#else
    if (handle_ != nullptr) {
        const int fd = static_cast<int>(reinterpret_cast<std::intptr_t>(handle_));
        ::flock(fd, LOCK_UN);
        ::close(fd);
    }
#endif
}

bool SingleInstance::IsFirstInstance() const {
    return handle_ != nullptr;
}

void SingleInstance::SignalOpenSettings() {
#ifdef _WIN32
    if (handle_ != nullptr) {
        return;
    }
    HANDLE event = OpenEventA(EVENT_MODIFY_STATE, FALSE, "Local\\LwfOpenSettingsEvent");
    if (event != nullptr) {
        SetEvent(event);
        CloseHandle(event);
    }
#endif
}
