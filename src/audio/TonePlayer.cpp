#include "audio/TonePlayer.h"

#include <miniaudio.h>

#include <algorithm>

namespace ToneGenerator {

namespace {

constexpr int kToneSampleRate = 16000;

}

static void ToneDataCallback(ma_device* device, void* output, const void*, std::uint32_t frames) {
    auto* state = static_cast<DeviceState*>(device->pUserData);
    if (state == nullptr) {
        return;
    }
    auto* out = static_cast<float*>(output);
    std::lock_guard<std::mutex> lock(state->mutex);
    for (std::uint32_t i = 0; i < frames; ++i) {
        if (state->position < state->active.size()) {
            out[i] = state->active[state->position++];
        } else {
            out[i] = 0.0f;
        }
    }
    if (state->position >= state->active.size() && !state->active.empty()) {
        state->active.clear();
        state->position = 0;
        state->done_cv.notify_all();
    }
}

TonePlayer::~TonePlayer() {
    Shutdown();
}

void TonePlayer::Play(const ToneSpec& spec, double volume) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            running_ = true;
            worker_ = std::thread([this] { WorkerLoop(); });
        }
        volume_ = volume;
        queue_.push_back(spec);
    }
    condition_.notify_one();
}

void TonePlayer::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ != nullptr) {
        ma_device_uninit(static_cast<ma_device*>(device_));
        delete static_cast<ma_device*>(device_);
        device_ = nullptr;
        device_ready_ = false;
        state_.reset();
    }
}

void TonePlayer::EnsureDeviceLocked() {
    if (device_ready_ || !running_) {
        return;
    }
    state_ = std::make_unique<DeviceState>();
    auto* device = new ma_device();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = kToneSampleRate;
    config.dataCallback = &ToneDataCallback;
    config.pUserData = state_.get();
    if (ma_device_init(nullptr, &config, device) != MA_SUCCESS) {
        delete device;
        state_.reset();
        return;
    }
    if (ma_device_start(device) != MA_SUCCESS) {
        ma_device_uninit(device);
        delete device;
        state_.reset();
        return;
    }
    device_ = device;
    device_ready_ = true;
}

void TonePlayer::WorkerLoop() {
    for (;;) {
        ToneSpec spec;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) {
                return;
            }
            if (queue_.empty()) {
                continue;
            }
            spec = queue_.front();
            queue_.pop_front();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            EnsureDeviceLocked();
        }
        if (!device_ready_) {
            continue;
        }

        const std::vector<float> samples =
            Generate(kToneSampleRate, spec.frequency_hz, spec.duration_ms, 10, volume_);
        {
            std::unique_lock<std::mutex> lock(state_->mutex);
            state_->done_cv.wait(lock, [this] { return state_->active.empty(); });
            state_->active = samples;
            state_->position = 0;
        }
    }
}
}
