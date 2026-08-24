#include "audio/MiniaudioCapture.h"

#include <stdexcept>
#include <string>

namespace {

std::string DescribeResult(ma_result result) {
    const char* description = ma_result_description(result);
    return description != nullptr ? description : "unknown error";
}

}

MiniaudioCapture::MiniaudioCapture(std::uint32_t sample_rate, std::uint32_t channels)
    : sample_rate_(sample_rate), channels_(channels) {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = &MiniaudioCapture::DataCallback;
    config.pUserData = this;

    const ma_result result = ma_device_init(nullptr, &config, &device_);
    if (result != MA_SUCCESS) {
        throw std::runtime_error("MiniaudioCapture: device init failed: " + DescribeResult(result));
    }
    device_initialized_ = true;
}

MiniaudioCapture::~MiniaudioCapture() {
    if (device_initialized_) {
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }
}

void MiniaudioCapture::Start() {
    if (!device_initialized_) {
        throw std::runtime_error("MiniaudioCapture: device not initialized");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_.clear();
    }
    capturing_.store(true);
    const ma_result result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        capturing_.store(false);
        throw std::runtime_error("MiniaudioCapture: device start failed: " + DescribeResult(result));
    }
}

void MiniaudioCapture::Stop() {
    if (!device_initialized_) {
        throw std::runtime_error("MiniaudioCapture: device not initialized");
    }
    capturing_.store(false);
    const ma_result result = ma_device_stop(&device_);
    if (result != MA_SUCCESS) {
        throw std::runtime_error("MiniaudioCapture: device stop failed: " + DescribeResult(result));
    }
}

std::vector<float> MiniaudioCapture::GetRecordedAudio() {
    std::lock_guard<std::mutex> lock(mutex_);
    return recording_;
}

void MiniaudioCapture::AppendFrames(const float* frames, ma_uint32 frame_count) {
    if (!capturing_.load(std::memory_order_relaxed)) {
        return;
    }
    const std::size_t sample_count = static_cast<std::size_t>(frame_count) * channels_;
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
