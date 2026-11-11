#pragma once

#include "core/IAudioCapture.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "miniaudio.h"

class MiniaudioCapture : public IAudioCapture {
public:
    explicit MiniaudioCapture(std::uint32_t sample_rate = 16000, std::uint32_t channels = 1,
                              std::string device_name = "");
    ~MiniaudioCapture() override;

    // Display names of the system's capture devices; {} when enumeration is
    // unavailable. Static: listing needs no instance (and no running capture).
    static std::vector<std::string> ListCaptureDevices();

    MiniaudioCapture(const MiniaudioCapture&) = delete;
    MiniaudioCapture& operator=(const MiniaudioCapture&) = delete;

    void Start() override;
    void Stop() override;
    std::vector<float> GetRecordedAudio() override;

    void SetChunkSink(IAudioChunkSink* sink) override;
    float AudioLevel() const noexcept override;
    int SampleRate() const override { return static_cast<int>(sample_rate_); }
    bool DeviceLost() const override { return device_lost_.load(std::memory_order_relaxed); }

private:
    static void DataCallback(ma_device* device, void* output, const void* input,
                             ma_uint32 frame_count);
    static void StopCallback(ma_device* device);

    void AppendFrames(const float* frames, ma_uint32 frame_count);

    std::uint32_t sample_rate_;
    std::uint32_t channels_;
    // INVARIANT: an owned context makes name->id selection possible; it must be
    // uninitted AFTER the device (dtor body order) because the device may use it.
    ma_context context_{};
    bool context_ok_ = false;
    ma_device device_{};
    bool device_initialized_ = false;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> device_lost_{false};
    std::atomic<float> level_{0.0f};
    IAudioChunkSink* chunk_sink_ = nullptr;
    std::mutex mutex_;
    std::vector<float> recording_;
};
