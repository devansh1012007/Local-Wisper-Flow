#include "audio/TonePlayer.h"

#include "core/Log.h"

#include <miniaudio.h>

#include <algorithm>
#include <cstdio>

namespace ToneGenerator {

namespace {

constexpr int kToneSampleRate = 16000;

// Followups-3 D1: cap on how long the worker waits for the previous tone to
// drain before replacing it. A healthy 160 ms tone drains in ~160 ms; only a
// dead/stalled audio callback hits the cap, and without it the worker would
// block forever and silently swallow every later tone (perceived as "the
// volume slider does nothing").
constexpr auto kToneDrainTimeout = std::chrono::milliseconds(2000);

std::string DescribeResult(ma_result result) {
    const char* description = ma_result_description(result);
    return description != nullptr ? description : "unknown error";
}

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
    // Followups-3 D1 diagnosability: every requested tone is logged (spec +
    // amplitude), so "the slider does nothing" reports become answerable
    // from the log ring alone.
    char line[128];
    std::snprintf(line, sizeof(line), "tone: %.1f Hz %d ms vol %.2f", spec.frequency_hz,
                  spec.duration_ms, volume);
    lwf::Log::Debug(line);
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
    const ma_result init_result = ma_device_init(nullptr, &config, device);
    if (init_result != MA_SUCCESS) {
        // Followups-3 D1: this used to fail with no trace at all, so a machine
        // with no usable output device dropped every tone in silence and the
        // volume slider looked broken. Log it and flag it for the UI.
        lwf::Log::Warning("TonePlayer: playback device init failed: " +
                          DescribeResult(init_result));
        delete device;
        state_.reset();
        device_failed_ = true;
        return;
    }
    const ma_result start_result = ma_device_start(device);
    if (start_result != MA_SUCCESS) {
        lwf::Log::Warning("TonePlayer: playback device start failed: " +
                          DescribeResult(start_result));
        ma_device_uninit(device);
        delete device;
        state_.reset();
        device_failed_ = true;
        return;
    }
    lwf::Log::Info("TonePlayer: playback device ready");
    device_ = device;
    device_ready_ = true;
    device_failed_ = false;
}

bool TonePlayer::DeviceFailed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return device_failed_;
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
            // Followups-3 D1: bounded drain wait. The old unbounded wait could
            // block the worker forever when the audio callback stopped firing
            // (dead endpoint mid-session), silently swallowing every later
            // tone; the cap replaces the stale buffer instead and logs it.
            if (!state_->done_cv.wait_for(lock, kToneDrainTimeout,
                                          [this] { return state_->active.empty(); })) {
                lwf::Log::Warning("TonePlayer: audio callback stalled; dropping stale tone");
                state_->active.clear();
            }
            state_->active = samples;
            state_->position = 0;
        }
    }
}

}
