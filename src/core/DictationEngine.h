#pragma once

#include "core/AudioPipe.h"
#include "core/DictationFSM.h"
#include "core/EngineEvent.h"
#include "core/IAudioCapture.h"
#include "core/IInferenceProvider.h"
#include "core/ITextInjector.h"
#include "core/IVadProvider.h"
#include "core/PersonalDictionary.h"
#include "core/VadGate.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>

enum class EngineErrorCode {
    None,
    MicUnavailable,
    MicLost,
    SttFailed,
    InjectFailed,
};

class DictationCleanup;

class IEngineSink {
public:
    virtual ~IEngineSink() = default;

    virtual void OnError(EngineErrorCode code, const std::string& detail) = 0;
    virtual void OnStatus(const std::string& text) = 0;
};

class EngineStatusSink final : public IEngineSink {
public:
    void OnError(EngineErrorCode code, const std::string& detail) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            error_ = detail;
        }
        last_error_.store(code, std::memory_order_relaxed);
        error_generation_.fetch_add(1, std::memory_order_relaxed);
    }

    void OnStatus(const std::string& text) override {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = text;
    }

    std::string ErrorText() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_;
    }

    std::string StatusText() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    EngineErrorCode LastError() const noexcept {
        return last_error_.load(std::memory_order_relaxed);
    }

    std::uint64_t ErrorGeneration() const noexcept {
        return error_generation_.load(std::memory_order_relaxed);
    }

private:
    mutable std::mutex mutex_;
    std::string status_;
    std::string error_;
    std::atomic<EngineErrorCode> last_error_{EngineErrorCode::None};
    std::atomic<std::uint64_t> error_generation_{0};
};

struct EngineSettings {
    std::uint32_t sample_rate = 16000;
    std::chrono::milliseconds min_hold{100};
    int hands_free_silence_ms = 0;
    std::chrono::milliseconds max_recording{30000};
    PersonalDictionary* dictionary = nullptr;
};

class DictationEngine {
public:
    using ChunkSink = std::function<void(const float*, std::size_t)>;

    DictationEngine(IAudioCapture& audio, IInferenceProvider& stt, DictationCleanup& cleanup,
                    ITextInjector& injector, DictationFSM& fsm, IVadProvider* vad,
                    EngineSettings settings, IEngineSink* sink = nullptr);

    void Run(EngineEventQueue& queue, const std::atomic<bool>& quit);

    ChunkSink Sink();

    std::string LastText() const;

private:
    enum class StopReason {
        HotkeyRelease,
        SilenceStop,
        MaxDuration,
    };

    void HandleEvent(const EngineEvent& event, const std::atomic<bool>& quit);
    void PollCapture();
    void DrainGate();
    void BeginCapture(const EngineEvent& press);
    void FinishCapture(StopReason reason, const EngineEvent& release_event);
    void FailToIdle(EngineErrorCode code, const std::string& detail);
    void TranscribeAndInject(const EngineEvent& release_event);

    IAudioCapture& audio_;
    IInferenceProvider& stt_;
    DictationCleanup& cleanup_;
    ITextInjector& injector_;
    DictationFSM& fsm_;
    EngineSettings settings_;
    IEngineSink* sink_;
    AudioPipe pipe_;
    VadGate gate_;
    std::chrono::steady_clock::time_point press_time_;
    mutable std::mutex text_mutex_;
    std::string last_text_;
};
