#include "audio/MiniaudioCapture.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::string DescribeResult(ma_result result) {
    const char* description = ma_result_description(result);
    return description != nullptr ? description : "unknown error";
}

constexpr float kLevelDecay = 0.8f;

}  // namespace

MiniaudioCapture::MiniaudioCapture(std::uint32_t sample_rate, std::uint32_t channels)
    : sample_rate_(sample_rate), channels_(channels), preroll_(sample_rate * 300 / 1000) {
    InitDevice();
}

MiniaudioCapture::~MiniaudioCapture() {
    shutting_down_.store(true, std::memory_order_relaxed);
    if (device_initialized_) {
        if (device_running_.load(std::memory_order_relaxed)) {
            ma_device_stop(&device_);
        }
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }
}

void MiniaudioCapture::InitDevice() {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = &MiniaudioCapture::DataCallback;
    config.notificationCallback = &MiniaudioCapture::NotificationCallback;
    config.pUserData = this;

    const ma_result result = ma_device_init(nullptr, &config, &device_);
    if (result != MA_SUCCESS) {
        throw std::runtime_error("MiniaudioCapture: device init failed: " + DescribeResult(result));
    }
    device_initialized_ = true;
    device_lost_.store(false, std::memory_order_relaxed);
    device_running_.store(false, std::memory_order_relaxed);
}

void MiniaudioCapture::RecoverDeviceIfNeeded() {
    if (!device_lost_.load(std::memory_order_relaxed)) {
        return;
    }
    if (device_initialized_) {
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }
    InitDevice();
    preroll_.Clear();
}

void MiniaudioCapture::SetChunkSink(std::function<void(const float*, std::size_t)> sink) {
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sink_ = std::move(sink);
}

void MiniaudioCapture::Start() {
    if (!device_initialized_) {
        throw std::runtime_error("MiniaudioCapture: device not initialized");
    }
    RecoverDeviceIfNeeded();

    if (!device_running_.load(std::memory_order_relaxed)) {
        const ma_result result = ma_device_start(&device_);
        if (result != MA_SUCCESS) {
            throw std::runtime_error("MiniaudioCapture: device start failed: " +
                                     DescribeResult(result));
        }
        device_running_.store(true, std::memory_order_relaxed);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_ = preroll_.Snapshot();
    }
    level_.store(0.0f, std::memory_order_relaxed);
    capturing_.store(true, std::memory_order_relaxed);
}

void MiniaudioCapture::Stop() {
    if (!device_initialized_) {
        throw std::runtime_error("MiniaudioCapture: device not initialized");
    }
    capturing_.store(false, std::memory_order_relaxed);
    level_.store(0.0f, std::memory_order_relaxed);
    if (device_running_.load(std::memory_order_relaxed)) {
        ma_device_stop(&device_);
        device_running_.store(false, std::memory_order_relaxed);
    }
}

float MiniaudioCapture::AudioLevel() const noexcept {
    return level_.load(std::memory_order_relaxed);
}

bool MiniaudioCapture::DeviceLost() const noexcept {
    return device_lost_.load(std::memory_order_relaxed);
}

std::vector<float> MiniaudioCapture::GetRecordedAudio() {
    std::lock_guard<std::mutex> lock(mutex_);
    return recording_;
}

void MiniaudioCapture::AppendFrames(const float* frames, ma_uint32 frame_count) {
    const std::size_t sample_count = static_cast<std::size_t>(frame_count) * channels_;
    if (sample_count == 0) {
        return;
    }

    float peak = 0.0f;
    for (std::size_t i = 0; i < sample_count; ++i) {
        peak = std::max(peak, std::fabs(frames[i]));
    }
    const float previous = level_.load(std::memory_order_relaxed);
    level_.store(std::max(peak, previous * kLevelDecay), std::memory_order_relaxed);

    preroll_.Write(frames, sample_count);

    {
        std::lock_guard<std::mutex> lock(sink_mutex_);
        if (sink_) {
            sink_(frames, sample_count);
        }
    }

    if (!capturing_.load(std::memory_order_relaxed)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    recording_.insert(recording_.end(), frames, frames + sample_count);
}

void MiniaudioCapture::DataCallback(ma_device* device, void*, const void* input,
                                    ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioCapture*>(device->pUserData);
    if (self == nullptr || input == nullptr || frame_count == 0) {
        return;
    }
    self->AppendFrames(static_cast<const float*>(input), frame_count);
}

void MiniaudioCapture::NotificationCallback(const ma_device_notification* notification) {
    if (notification == nullptr) {
        return;
    }
    auto* self = static_cast<MiniaudioCapture*>(notification->pDevice->pUserData);
    if (self == nullptr) {
        return;
    }
    if (notification->type == ma_device_notification_type_stopped &&
        !self->shutting_down_.load(std::memory_order_relaxed)) {
        self->device_running_.store(false, std::memory_order_relaxed);
        self->device_lost_.store(true, std::memory_order_relaxed);
        self->capturing_.store(false, std::memory_order_relaxed);
    }
}
