#include "os/SingleInstance.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

std::string SanitizedMutexName(std::string name) {
    for (char& c : name) {
        if (c == '\\' || c == '/') {
            c = '_';
        }
    }
    return "Local\\" + name;
}

}

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
    const std::string lock_path = "/tmp/" + SanitizedMutexName(mutex_name) + ".lock";
    handle_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(open(lock_path.c_str(), O_CREAT | O_RDWR, 0666)));
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
        close(static_cast<std::intptr_t>(reinterpret_cast<std::ptrdiff_t>(handle_)));
    }
#endif
}

bool SingleInstance::IsFirstInstance() const {
#ifdef _WIN32
    return handle_ != nullptr;
#else
    return handle_ != nullptr;
#endif
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
