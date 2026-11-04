#pragma once

#include "core/ErrorCode.h"
#include "core/RateLimiter.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

class UiBus;
class SettingsStore;

class WindowsTray {
public:
    WindowsTray(UiBus& bus, SettingsStore& settings);
    ~WindowsTray();

    WindowsTray(const WindowsTray&) = delete;
    WindowsTray& operator=(const WindowsTray&) = delete;

    void Start();
    void Stop();
    void NotifyError(ErrorCode code, const std::string& detail);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

namespace Autostart {

bool IsEnabled();
bool SetEnabled(bool enabled);

}
