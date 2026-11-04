#pragma once

#include "core/DictationFSM.h"
#include "core/EngineTypes.h"
#include "core/EventQueue.h"
#include "core/IAudioCapture.h"
#include "core/IEngineSink.h"
#include "core/IInferenceProvider.h"
#include "core/ITextInjector.h"
#include "core/IVadProvider.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class IClock;
class TextPipeline;
class CommandModeController;
class IToneFeedback;
class StreamingDictator;

namespace ToneGenerator {
struct ToneSpec;
}

class DictationEngine final : public IAudioChunkSink {
public:
    DictationEngine(IAudioCapture& capture, IVadProvider* vad, IInferenceProvider& stt,
                    TextPipeline& pipeline, ITextInjector& injector, DictationFSM& fsm,
                    const IClock& clock, IEngineSink& sink, const EngineSettings& settings);
    ~DictationEngine() override;

    DictationEngine(const DictationEngine&) = delete;
    DictationEngine& operator=(const DictationEngine&) = delete;

    void Start();
    void Stop();
    void Run();

    bool ProcessOne(int timeout_ms);
    void Tick();

    void PostEvent(EngineEventType type);
    void RequestCancel();

    void OnAudioChunk(const float* samples, std::size_t frame_count) override;
    float AudioLevel() const noexcept;

    void SetHandsFree(bool enabled);
    void SetCommandController(CommandModeController* controller);
    void SetToneFeedback(IToneFeedback* tones);
    std::string LastText() const;
    std::size_t StreamedSegments() const;

private:
    void HandleEvent(const EngineEvent& event);
    void HandlePress(std::int64_t now_ms);
    void HandleRelease(std::int64_t now_ms);
    void HandleToggle(std::int64_t now_ms);
    void HandleCancel();
    void BeginCapture(std::int64_t now_ms);
    void StopAndTranscribe(std::int64_t now_ms, bool timeout_fired);
    void HandleCommandMode(std::int64_t now_ms);
    void HandlePasteLast();
    void EmitError(ErrorCode code, const std::string& detail);
    void PlayTone(const ToneGenerator::ToneSpec& spec);
    void DiscardCapture(const char* reason);
    void ReturnToIdle(const char* status);
    void HandleMicLost();
    void MaybeCommitStreamingSegment();
    std::vector<float> TakeTail();

    IAudioCapture& capture_;
    IVadProvider* vad_;
    IInferenceProvider& stt_;
    TextPipeline& pipeline_;
    ITextInjector& injector_;
    DictationFSM& fsm_;
    const IClock& clock_;
    IEngineSink& sink_;
    EngineSettings settings_;

    EventQueue<EngineEvent> queue_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> speech_started_{false};
    std::atomic<bool> hands_free_{false};
    std::atomic<bool> in_command_mode_{false};
    std::atomic<float> audio_level_{0.0f};
    std::atomic<std::int64_t> total_samples_{0};
    std::atomic<std::int64_t> silence_start_samples_{-1};
    std::atomic<std::int64_t> committed_samples_{0};
    std::atomic<bool> speech_seen_{false};
    std::unique_ptr<StreamingDictator> streamer_;
    CommandModeController* command_ = nullptr;
    IToneFeedback* tones_ = nullptr;

    std::int64_t press_ms_ = 0;
    std::int64_t capture_start_ms_ = 0;
    std::int64_t last_speech_ms_ = 0;
    std::size_t chunk_count_ = 0;
    std::size_t speech_chunk_count_ = 0;
    mutable std::mutex text_mutex_;
    std::string last_text_;
};
