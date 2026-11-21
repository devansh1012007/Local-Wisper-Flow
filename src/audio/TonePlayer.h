#pragma once

#include "core/IToneFeedback.h"
#include "core/ToneGenerator.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace ToneGenerator {

struct DeviceState;

class TonePlayer final : public IToneFeedback {
public:
    TonePlayer() = default;
    ~TonePlayer();

    TonePlayer(const TonePlayer&) = delete;
    TonePlayer& operator=(const TonePlayer&) = delete;

    void Play(const ToneSpec& spec, double volume) override;
    void Shutdown();

private:
    void WorkerLoop();
    void EnsureDeviceLocked();

    std::thread worker_;
    std::deque<ToneSpec> queue_;
    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool running_ = false;
    bool device_ready_ = false;
    void* device_ = nullptr;
    std::unique_ptr<DeviceState> state_;
    double volume_ = 0.3;
};

}
