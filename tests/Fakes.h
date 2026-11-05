#pragma once

#include "core/IAudioCapture.h"
#include "core/IClock.h"
#include "core/IEngineSink.h"
#include "core/IInferenceProvider.h"
#include "core/ILlmProvider.h"
#include "core/ITextInjector.h"
#include "core/IVadProvider.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class FakeAudioCapture final : public IAudioCapture {
public:
    void Start() override {
        if (fail_start) {
            throw std::runtime_error("device start failed");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        recording_.clear();
        started = true;
        start_count.fetch_add(1);
    }

    void Stop() override {
        if (fail_stop) {
            throw std::runtime_error("device stop failed");
        }
        started = false;
    }

    std::vector<float> GetRecordedAudio() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return recording_;
    }

    // Current IAudioCapture sink contract: the engine registers itself as an
    // IAudioChunkSink*; FeedSamples pushes chunks through it.
    void SetChunkSink(IAudioChunkSink* sink) override {
        std::lock_guard<std::mutex> lock(mutex_);
        sink_ = sink;
    }

    bool DeviceLost() const noexcept override {
        return device_lost;
    }

    void FeedSamples(const std::vector<float>& samples) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recording_.insert(recording_.end(), samples.begin(), samples.end());
        }
        if (sink_) {
            sink_->OnAudioChunk(samples.data(), samples.size());
        }
    }

    bool fail_start = false;
    bool fail_stop = false;
    std::atomic<bool> device_lost{false};
    std::atomic<bool> started{false};
    std::atomic<std::size_t> start_count{0};

private:
    std::mutex mutex_;
    std::vector<float> recording_;
    IAudioChunkSink* sink_ = nullptr;
};

class ScriptedVad final : public IVadProvider {
public:
    explicit ScriptedVad(std::vector<bool> script) : script_(std::move(script)) {}

    bool IsSpeech(const std::vector<float>& audio_chunk) override {
        const std::size_t index = call_count.fetch_add(1);
        std::lock_guard<std::mutex> lock(mutex_);
        chunk_sizes.push_back(audio_chunk.size());
        if (index >= script_.size()) {
            return script_.empty() ? false : script_.back();
        }
        return script_[index];
    }

    void Reset() {
        reset_count.fetch_add(1);
    }

    // Pin the VAD to a constant speech/silence answer regardless of the script.
    void SetSpeech(bool speech) {
        std::lock_guard<std::mutex> lock(mutex_);
        script_.assign(1, speech);
    }

    std::atomic<std::size_t> reset_count{0};
    std::atomic<std::size_t> call_count{0};
    std::vector<std::size_t> chunk_sizes;

private:
    std::mutex mutex_;
    std::vector<bool> script_;
};

class FakeStt final : public IInferenceProvider {
public:
    std::string Transcribe(const std::vector<float>& audio_pcm) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            transcribed_sizes.push_back(audio_pcm.size());
        }
        if (on_transcribe) {
            on_transcribe();
        }
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        if (fail_transcribe) {
            throw std::runtime_error("stt failure");
        }
        return transcript;
    }

    // Records the rolling prompt passed by the streaming path, then delegates to
    // Transcribe exactly like the default IInferenceProvider implementation.
    std::string TranscribeWithPrompt(const std::vector<float>& audio_pcm,
                                     const std::string& prompt) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            prompts.push_back(prompt);
        }
        return Transcribe(audio_pcm);
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        return text;
    }

    std::string transcript = "hello world from the test";
    bool fail_transcribe = false;
    int delay_ms = 0;
    std::vector<std::size_t> transcribed_sizes;
    std::vector<std::string> prompts;
    // Synchronous hook invoked inside Transcribe (used to inject a cancel mid-call).
    std::function<void()> on_transcribe;

    std::size_t LastTranscribedSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return transcribed_sizes.empty() ? 0 : transcribed_sizes.back();
    }

    std::size_t TranscribeCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return transcribed_sizes.size();
    }

private:
    std::mutex mutex_;
};

class RecordingInjector final : public ITextInjector {
public:
    void InjectText(const std::string& text) override {
        if (fail_inject) {
            throw std::runtime_error("inject failure");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        injected.push_back(text);
    }

    bool HasInjected() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !injected.empty();
    }

    std::string LastInjected() {
        std::lock_guard<std::mutex> lock(mutex_);
        return injected.empty() ? std::string() : injected.back();
    }

    std::size_t InjectionCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return injected.size();
    }

    // Returns a copy of every injected string.
    std::vector<std::string> Injected() {
        std::lock_guard<std::mutex> lock(mutex_);
        return injected;
    }

    void ClearInjected() {
        std::lock_guard<std::mutex> lock(mutex_);
        injected.clear();
    }

    bool fail_inject = false;

private:
    std::mutex mutex_;
    std::vector<std::string> injected;
};

class ScriptedLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string& system_prompt) override {
        std::lock_guard<std::mutex> lock(mutex_);
        calls.push_back(text);
        prompts.push_back(system_prompt);
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        if (cancelled_earlier && cancel_count.load() > 0) {
            throw std::runtime_error("cancelled");
        }
        if (fail) {
            throw std::runtime_error("llm failure");
        }
        if (truncated_mode) {
            last_truncated = true;
        }
        return response.empty() ? text : response;
    }

    void RequestCancel() override {
        cancel_count.fetch_add(1);
    }

    // ILlmProvider carries no truncation query today; kept as a plain helper for
    // tests that script truncated_mode.
    bool LastGenerationTruncated() const {
        return last_truncated;
    }

    std::string response;
    int delay_ms = 0;
    bool fail = false;
    bool truncated_mode = false;
    bool cancelled_earlier = false;
    std::atomic<int> cancel_count{0};
    mutable bool last_truncated = false;
    std::vector<std::string> calls;
    std::vector<std::string> prompts;

    std::size_t CallCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return calls.size();
    }

private:
    std::mutex mutex_;
};

class FakeClock final : public IClock {
public:
    std::int64_t NowMs() const override {
        return now_ms_;
    }

    void Advance(std::int64_t delta_ms) {
        now_ms_ += delta_ms;
    }

private:
    std::int64_t now_ms_ = 0;
};

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override {
        return text;
    }
};

class RecordingSink final : public IEngineSink {
public:
    void OnStatus(const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        statuses.push_back(message);
    }

    void OnError(ErrorCode code, const std::string& detail) override {
        std::lock_guard<std::mutex> lock(mutex_);
        errors.push_back(code);
        error_details.push_back(detail);
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        statuses.clear();
        errors.clear();
        error_details.clear();
    }

    std::vector<std::string> statuses;
    std::vector<ErrorCode> errors;
    std::vector<std::string> error_details;

private:
    std::mutex mutex_;
};

inline std::vector<float> MakeSpeech(std::size_t samples) {
    std::vector<float> out(samples, 0.0f);
    for (std::size_t i = 0; i < samples; ++i) {
        out[i] = 0.4f * static_cast<float>(std::sin(static_cast<double>(i) * 0.05));
    }
    return out;
}

inline std::vector<float> MakeSilence(std::size_t samples) {
    return std::vector<float>(samples, 0.0001f);
}
