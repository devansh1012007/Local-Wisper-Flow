#pragma once

#include "audio/PreRollBuffer.h"
#include "core/IAudioCapture.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "miniaudio.h"

class MiniaudioCapture : public IAudioCapture {
public:
    explicit MiniaudioCapture(std::uint32_t sample_rate = 16000, std::uint32_t channels = 1);
    ~MiniaudioCapture() override;

    MiniaudioCapture(const MiniaudioCapture&) = delete;
    MiniaudioCapture& operator=(const MiniaudioCapture&) = delete;

    void Start() override;
    void Stop() override;
    std::vector<float> GetRecordedAudio() override;
    void SetChunkSink(std::function<void(const float*, std::size_t)> sink) override;
    float AudioLevel() const noexcept override;
    bool DeviceLost() const noexcept override;

private:
    static void DataCallback(ma_device* device, void* output, const void* input,
                             ma_uint32 frame_count);
    static void NotificationCallback(const ma_device_notification* notification);

    void AppendFrames(const float* frames, ma_uint32 frame_count);
    void InitDevice();
    void RecoverDeviceIfNeeded();

    std::uint32_t sample_rate_;
    std::uint32_t channels_;
    ma_device device_{};
    bool device_initialized_ = false;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> device_running_{false};
    std::atomic<bool> device_lost_{false};
    std::atomic<bool> shutting_down_{false};
    std::atomic<float> level_{0.0f};
    std::mutex mutex_;
    std::vector<float> recording_;
    std::mutex sink_mutex_;
    std::function<void(const float*, std::size_t)> sink_;
    PreRollBuffer preroll_;
};
