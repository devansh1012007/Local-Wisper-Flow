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

    // Followups-3 D1 diagnosability: true once a tone was requested and the
    // playback device failed to open (every tone since is being dropped).
    // main.cpp polls this to surface a one-shot user-visible notice, so a
    // machine with a dead output endpoint no longer presents as "the volume
    // slider does nothing".
    bool DeviceFailed() const;

private:
    void WorkerLoop();
    void EnsureDeviceLocked();

    std::thread worker_;
    std::deque<ToneSpec> queue_;
    std::condition_variable condition_;
    mutable std::mutex mutex_;
    bool running_ = false;
    bool device_ready_ = false;
    bool device_failed_ = false;  // guarded by mutex_
    void* device_ = nullptr;
    std::unique_ptr<DeviceState> state_;
    double volume_ = 0.3;
};

}
