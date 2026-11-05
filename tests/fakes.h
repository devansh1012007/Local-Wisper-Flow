#pragma once

#include "core/IAudioCapture.h"
#include "core/IInferenceProvider.h"
#include "core/ILlmProvider.h"
#include "core/ITextInjector.h"
#include "core/IVadProvider.h"

#include <atomic>
#include <chrono>
#include <cmath>
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

    void SetChunkSink(std::function<void(const float*, std::size_t)> sink) override {
        sink_ = std::move(sink);
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
            sink_(samples.data(), samples.size());
        }
    }

    bool fail_start = false;
    bool fail_stop = false;
    std::atomic<bool> device_lost{false};
    std::atomic<bool> started{false};

private:
    std::mutex mutex_;
    std::vector<float> recording_;
    std::function<void(const float*, std::size_t)> sink_;
};

class ScriptedVad final : public IVadProvider {
public:
    explicit ScriptedVad(std::vector<bool> script) : script_(std::move(script)) {}

    bool IsSpeech(const std::vector<float>& audio_chunk) override {
        const std::size_t index = call_count.fetch_add(1);
        chunk_sizes.push_back(audio_chunk.size());
        if (index >= script_.size()) {
            return script_.empty() ? false : script_.back();
        }
        return script_[index];
    }

    void Reset() override {
        reset_count.fetch_add(1);
    }

    std::atomic<std::size_t> reset_count{0};
    std::atomic<std::size_t> call_count{0};
    std::vector<std::size_t> chunk_sizes;

private:
    std::vector<bool> script_;
};

class FakeStt final : public IInferenceProvider {
public:
    std::string Transcribe(const std::vector<float>& audio_pcm) override {
        std::lock_guard<std::mutex> lock(mutex_);
        transcribed_sizes.push_back(audio_pcm.size());
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        if (fail_transcribe) {
            throw std::runtime_error("stt failure");
        }
        return transcript;
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        return text;
    }

    std::string transcript = "hello world from the test";
    bool fail_transcribe = false;
    int delay_ms = 0;
    std::vector<std::size_t> transcribed_sizes;

    std::size_t LastTranscribedSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return transcribed_sizes.empty() ? 0 : transcribed_sizes.back();
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
        if (truncated_mode) {
            last_truncated = true;
        }
        return response.empty() ? text : response;
    }

    void RequestCancel() override {
        cancel_count.fetch_add(1);
    }

    bool LastGenerationTruncated() const override {
        return last_truncated;
    }

    std::string response;
    int delay_ms = 0;
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
