#pragma once

#include "core/ErrorCode.h"
#include "core/IAudioCapture.h"
#include "core/IClock.h"
#include "core/IEngineSink.h"
#include "core/ILlmProvider.h"
#include "core/IInferenceProvider.h"
#include "core/ITextInjector.h"
#include "core/IVadProvider.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace lwf_test {

class FakeClock final : public IClock {
public:
    std::int64_t now_ms = 1'000'000;

    std::int64_t NowMs() const override { return now_ms; }
    void Advance(std::int64_t ms) { now_ms += ms; }
};

class FakeAudioCapture : public IAudioCapture {
public:
    void Start() override {
        ++start_count;
        buffer.clear();
        capturing = true;
    }

    void Stop() override {
        ++stop_count;
        capturing = false;
    }

    std::vector<float> GetRecordedAudio() override { return buffer; }

    void SetChunkSink(IAudioChunkSink* sink) override { sink_ = sink; }

    int SampleRate() const override { return sample_rate; }

    void Append(const std::vector<float>& samples) {
        buffer.insert(buffer.end(), samples.begin(), samples.end());
        if (sink_ != nullptr) {
            sink_->OnAudioChunk(samples.data(), samples.size());
        }
    }

    std::vector<float> buffer;
    int start_count = 0;
    int stop_count = 0;
    bool capturing = false;
    int sample_rate = 16000;

private:
    IAudioChunkSink* sink_ = nullptr;
};

class ScriptedVad final : public IVadProvider {
public:
    explicit ScriptedVad(bool speech) : speech_(speech) {}

    bool IsSpeech(const std::vector<float>&) override { return speech_; }

    void SetSpeech(bool speech) { speech_ = speech; }

private:
    std::atomic<bool> speech_;
};

class FakeStt final : public IInferenceProvider {
public:
    std::string transcript = "  hello   world  ";
    bool fail_transcribe = false;
    int transcribe_count = 0;

    std::string Transcribe(const std::vector<float>&) override {
        ++transcribe_count;
        if (fail_transcribe) {
            throw std::runtime_error("stt exploded");
        }
        if (on_transcribe) {
            on_transcribe();
        }
        return transcript;
    }

    std::string TranscribeWithPrompt(const std::vector<float>& pcm, const std::string& prompt) override {
        prompts.push_back(prompt);
        return Transcribe(pcm);
    }

    std::string ProcessText(const std::string& text, const std::string&) override { return text; }

    std::function<void()> on_transcribe;
    std::vector<std::string> prompts;
};

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override { return text; }
};

class RecordingInjector final : public ITextInjector {
public:
    bool fail = false;
    std::vector<std::string> injected;

    void InjectText(const std::string& text) override {
        if (fail) {
            throw std::runtime_error("injector exploded");
        }
        injected.push_back(text);
    }
};

class RecordingSink final : public IEngineSink {
public:
    std::vector<std::string> statuses;
    std::vector<ErrorCode> errors;
    std::vector<std::string> error_details;

    void OnStatus(const std::string& message) override { statuses.push_back(message); }

    void OnError(ErrorCode code, const std::string& detail) override {
        errors.push_back(code);
        error_details.push_back(detail);
    }
};

class ScriptedLlm final : public ILlmProvider {
public:
    std::string reply;
    std::string last_prompt;
    std::string last_text;
    int call_count = 0;
    bool fail = false;

    std::string Process(const std::string& text, const std::string& system_prompt) override {
        ++call_count;
        last_text = text;
        last_prompt = system_prompt;
        if (fail) {
            throw std::runtime_error("llm exploded");
        }
        return reply;
    }
};

}
