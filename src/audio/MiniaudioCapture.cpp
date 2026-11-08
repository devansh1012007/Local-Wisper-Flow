#include "audio/MiniaudioCapture.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/Log.h"

namespace {

std::string DescribeResult(ma_result result) {
    const char* description = ma_result_description(result);
    return description != nullptr ? description : "unknown error";
}

constexpr float kLevelDecay = 0.8f;

// RAII pairing for ma_context_init/ma_context_uninit: miniaudio requires every
// successful init to be matched, including when a later step returns early.
struct ContextGuard {
    ma_context* context;
    ~ContextGuard() {
        if (context != nullptr) {
            ma_context_uninit(context);
            context = nullptr;
        }
    }
};

}

MiniaudioCapture::MiniaudioCapture(std::uint32_t sample_rate, std::uint32_t channels,
                                   std::string device_name)
    : sample_rate_(sample_rate), channels_(channels) {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = &MiniaudioCapture::DataCallback;
    config.stopCallback = &MiniaudioCapture::StopCallback;
    config.pUserData = this;

    // DECISION: an owned context is what makes name->id selection possible, but if it
    // fails we keep the legacy path (ma_device_init with a null context self-manages
    // one) so startup behavior degrades to exactly what it was before this feature.
    const ma_result context_result = ma_context_init(nullptr, 0, nullptr, &context_);
    if (context_result == MA_SUCCESS) {
        context_ok_ = true;
    } else {
        lwf::Log::Warning("MiniaudioCapture: context init failed: " + DescribeResult(context_result) +
                          "; using system default capture device");
    }

    // INVARIANT: selected_id is copied out of the context-owned enumeration buffer so
    // it stays valid for the whole ma_device_init call (which copies it into the
    // device: MA_COPY_MEMORY of capture.id in ma_device_init). Duplicate display
    // names resolve to the first capture match.
    ma_device_id selected_id{};
    std::string active_name = "system default";
    bool selected = false;
    if (!device_name.empty() && context_ok_) {
        ma_device_info* capture_infos = nullptr;
        ma_uint32 capture_count = 0;
        const ma_result enum_result =
            ma_context_get_devices(&context_, nullptr, nullptr, &capture_infos, &capture_count);
        if (enum_result != MA_SUCCESS) {
            lwf::Log::Warning("MiniaudioCapture: device enumeration failed: " +
                              DescribeResult(enum_result) + "; using system default capture device");
        } else {
            for (ma_uint32 i = 0; i < capture_count; ++i) {
                if (device_name == capture_infos[i].name) {
                    selected_id = capture_infos[i].id;
                    selected = true;
                    active_name = capture_infos[i].name;
                    break;
                }
            }
            if (!selected) {
                lwf::Log::Warning("MiniaudioCapture: capture device '" + device_name +
                                  "' not found; using system default capture device");
            }
        }
    } else if (!device_name.empty()) {
        lwf::Log::Warning("MiniaudioCapture: requested capture device '" + device_name +
                          "' unavailable; using system default capture device");
    }

    if (selected) {
        config.capture.pDeviceID = &selected_id;
    }

    ma_result result = ma_device_init(context_ok_ ? &context_ : nullptr, &config, &device_);
    if (result != MA_SUCCESS && selected) {
        // AC1: a stale or rejected device id must never stop the app from starting.
        lwf::Log::Warning("MiniaudioCapture: init with device '" + active_name +
                          "' failed: " + DescribeResult(result) + "; retrying system default");
        config.capture.pDeviceID = nullptr;
        active_name = "system default";
        result = ma_device_init(context_ok_ ? &context_ : nullptr, &config, &device_);
    }
    if (result != MA_SUCCESS) {
        throw std::runtime_error("MiniaudioCapture: device init failed: " + DescribeResult(result));
    }
    device_initialized_ = true;
    // Diagnosability: which endpoint actually feeds the engine (name only, never audio).
    lwf::Log::Info("MiniaudioCapture: active capture device: " + active_name);
}

MiniaudioCapture::~MiniaudioCapture() {
    // INVARIANT: device first, then context -- ma_device_uninit touches the context,
    // and it only releases the context itself when it owns it (isOwnerOfContext,
    // set only by the null-context init path), so no double-uninit either way.
    if (device_initialized_) {
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }
    if (context_ok_) {
        ma_context_uninit(&context_);
        context_ok_ = false;
    }
}

std::vector<std::string> MiniaudioCapture::ListCaptureDevices() {
    ma_context context{};
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        return {};
    }
    ContextGuard guard{&context};

    ma_device_info* capture_infos = nullptr;
    ma_uint32 capture_count = 0;
    if (ma_context_get_devices(&context, nullptr, nullptr, &capture_infos, &capture_count) !=
        MA_SUCCESS) {
        return {};
    }

    std::vector<std::string> names;
    names.reserve(capture_count);
    for (ma_uint32 i = 0; i < capture_count; ++i) {
        names.emplace_back(capture_infos[i].name);
    }
    return names;
}

void MiniaudioCapture::Start() {
    if (!device_initialized_) {
        throw std::runtime_error("MiniaudioCapture: device not initialized");
    }
    device_lost_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_.clear();
    }
    capturing_.store(true);
    level_.store(0.0f, std::memory_order_relaxed);
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
    level_.store(0.0f, std::memory_order_relaxed);
    const ma_result result = ma_device_stop(&device_);
    if (result != MA_SUCCESS) {
        throw std::runtime_error("MiniaudioCapture: device stop failed: " + DescribeResult(result));
    }
    device_lost_.store(false, std::memory_order_relaxed);
}

float MiniaudioCapture::AudioLevel() const noexcept {
    return level_.load(std::memory_order_relaxed);
}

void MiniaudioCapture::SetChunkSink(IAudioChunkSink* sink) {
    chunk_sink_ = sink;
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

    if (!capturing_.load(std::memory_order_relaxed)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_.insert(recording_.end(), frames, frames + sample_count);
    }
    if (chunk_sink_ != nullptr) {
        chunk_sink_->OnAudioChunk(frames, frame_count);
    }
}

void MiniaudioCapture::DataCallback(ma_device* device, void*, const void* input,
                                    ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioCapture*>(device->pUserData);
    if (self == nullptr || input == nullptr || frame_count == 0) {
        return;
    }
    self->AppendFrames(static_cast<const float*>(input), frame_count);
}

void MiniaudioCapture::StopCallback(ma_device* device) {
    auto* self = static_cast<MiniaudioCapture*>(device->pUserData);
    if (self == nullptr) {
        return;
    }
    if (self->capturing_.load(std::memory_order_relaxed)) {
        self->device_lost_.store(true, std::memory_order_relaxed);
    }
}
