#include "core/DictationEngine.h"

#include "core/DictationCleanup.h"

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

namespace {

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

}  // namespace

DictationEngine::DictationEngine(IAudioCapture& audio, IInferenceProvider& stt,
                                 DictationCleanup& cleanup, ITextInjector& injector,
                                 DictationFSM& fsm, IVadProvider* vad, EngineSettings settings,
                                 IEngineSink* sink)
    : audio_(audio),
      stt_(stt),
      cleanup_(cleanup),
      injector_(injector),
      fsm_(fsm),
      settings_(settings),
      sink_(sink),
      gate_(vad) {}

DictationEngine::ChunkSink DictationEngine::Sink() {
    return [this](const float* samples, std::size_t count) { pipe_.Write(samples, count); };
}

std::string DictationEngine::LastText() const {
    std::lock_guard<std::mutex> lock(text_mutex_);
    return last_text_;
}

void DictationEngine::Run(EngineEventQueue& queue, const std::atomic<bool>& quit) {
    while (!quit.load()) {
        std::optional<EngineEvent> event = queue.PopFor(std::chrono::milliseconds(30));
        if (event) {
            HandleEvent(*event, quit);
            continue;
        }
        if (fsm_.CurrentState() == State::CAPTURING) {
            PollCapture();
        } else {
            pipe_.ReadAll();
        }
    }
}

void DictationEngine::HandleEvent(const EngineEvent& event, const std::atomic<bool>& quit) {
    switch (event.type) {
        case EngineEventType::Quit:
            return;
        case EngineEventType::PttPress:
            if (fsm_.CurrentState() != State::IDLE) {
                return;
            }
            BeginCapture(event);
            return;
        case EngineEventType::PttRelease:
            if (fsm_.CurrentState() != State::CAPTURING) {
                return;
            }
            FinishCapture(StopReason::HotkeyRelease, event);
            return;
        case EngineEventType::Cancel:
            if (fsm_.CurrentState() == State::CAPTURING) {
                try {
                    audio_.Stop();
                } catch (const std::exception& e) {
                    FailToIdle(EngineErrorCode::MicLost, e.what());
                    return;
                }
                pipe_.SetEnabled(false);
                pipe_.Clear();
                fsm_.TriggerEvent(Trigger::HOTKEY_RELEASE);
                fsm_.TriggerEvent(Trigger::SST_FAILED);
                std::cout << "[engine] cancelled" << std::endl;
            }
            return;
    }
}

void DictationEngine::BeginCapture(const EngineEvent& press) {
    if (!fsm_.TriggerEvent(Trigger::HOTKEY_PRESS)) {
        return;
    }
    fsm_.TriggerEvent(Trigger::VAD_SPEECH);
    press_time_ = press.timestamp;
    gate_.Reset();
    pipe_.Clear();
    pipe_.SetEnabled(true);
    try {
        audio_.Start();
    } catch (const std::exception& e) {
        pipe_.SetEnabled(false);
        fsm_.TriggerEvent(Trigger::HOTKEY_RELEASE);
        FailToIdle(EngineErrorCode::MicUnavailable, e.what());
        return;
    }
    std::cout << "[engine] capturing" << std::endl;
}

void DictationEngine::PollCapture() {
    if (audio_.DeviceLost()) {
        try {
            audio_.Stop();
        } catch (const std::exception&) {
        }
        fsm_.TriggerEvent(Trigger::HOTKEY_RELEASE);
        FailToIdle(EngineErrorCode::MicLost, "microphone device stopped");
        return;
    }
    DrainGate();
    if (!gate_.SpeechDetected()) {
        return;
    }
    if (settings_.hands_free_silence_ms > 0 &&
        gate_.SilenceMs(settings_.sample_rate) >=
            std::chrono::milliseconds(settings_.hands_free_silence_ms)) {
        FinishCapture(StopReason::SilenceStop,
                      EngineEvent{EngineEventType::PttRelease, std::chrono::steady_clock::now()});
        return;
    }
    if (gate_.DurationMs(settings_.sample_rate) >= settings_.max_recording) {
        FinishCapture(StopReason::MaxDuration,
                      EngineEvent{EngineEventType::PttRelease, std::chrono::steady_clock::now()});
    }
}

void DictationEngine::DrainGate() {
    const std::vector<float> samples = pipe_.ReadAll();
    if (!samples.empty()) {
        gate_.Feed(samples.data(), samples.size());
    }
}

void DictationEngine::FinishCapture(StopReason reason, const EngineEvent& release_event) {
    pipe_.SetEnabled(false);
    DrainGate();
    try {
        audio_.Stop();
    } catch (const std::exception& e) {
        fsm_.TriggerEvent(Trigger::HOTKEY_RELEASE);
        FailToIdle(EngineErrorCode::MicLost, e.what());
        return;
    }
    switch (reason) {
        case StopReason::HotkeyRelease:
            fsm_.TriggerEvent(Trigger::HOTKEY_RELEASE);
            break;
        case StopReason::SilenceStop:
            fsm_.TriggerEvent(Trigger::VAD_SILENCE);
            break;
        case StopReason::MaxDuration:
            fsm_.TriggerEvent(Trigger::TIMEOUT);
            break;
    }
    TranscribeAndInject(release_event);
}

void DictationEngine::FailToIdle(EngineErrorCode code, const std::string& detail) {
    if (sink_ != nullptr) {
        sink_->OnError(code, detail);
    }
    std::cerr << "[engine] error: " << detail << '\n';
    fsm_.TriggerEvent(Trigger::SST_FAILED);
    pipe_.SetEnabled(false);
    pipe_.Clear();
}

void DictationEngine::TranscribeAndInject(const EngineEvent& release_event) {
    const std::chrono::milliseconds held =
        std::chrono::duration_cast<std::chrono::milliseconds>(release_event.timestamp -
                                                              press_time_);
    if (held < settings_.min_hold) {
        std::cout << "[engine] tap shorter than " << settings_.min_hold.count()
                  << " ms ignored" << std::endl;
        fsm_.TriggerEvent(Trigger::SST_FAILED);
        return;
    }

    std::vector<float> pcm = audio_.GetRecordedAudio();
    if (!gate_.SpeechDetected()) {
        FailToIdle(EngineErrorCode::SttFailed, "no speech detected");
        return;
    }
    const std::pair<std::size_t, std::size_t> interval = gate_.SpeechInterval(pcm.size());
    if (interval.second > interval.first) {
        pcm = std::vector<float>(pcm.begin() + interval.first, pcm.begin() + interval.second);
    }
    if (pcm.empty()) {
        std::cout << "[engine] nothing heard" << std::endl;
        fsm_.TriggerEvent(Trigger::SST_FAILED);
        return;
    }

    std::string text;
    try {
        std::cout << "[stt] transcribing " << (pcm.size() * 1000 / settings_.sample_rate)
                  << " ms of audio" << std::endl;
        text = TrimCopy(stt_.Transcribe(pcm));
    } catch (const std::exception& e) {
        FailToIdle(EngineErrorCode::SttFailed, e.what());
        return;
    }
    if (text.empty()) {
        std::cout << "[engine] nothing heard" << std::endl;
        fsm_.TriggerEvent(Trigger::SST_FAILED);
        return;
    }

    fsm_.TriggerEvent(Trigger::SST_COMPLETE);
    const std::string processed =
        settings_.dictionary != nullptr ? settings_.dictionary->Enforce(cleanup_.Clean(text))
                                       : cleanup_.Clean(text);
    fsm_.TriggerEvent(Trigger::SST_COMPLETE);

    try {
        injector_.InjectText(processed);
    } catch (const std::exception& e) {
        FailToIdle(EngineErrorCode::InjectFailed, e.what());
        return;
    }
    {
        std::lock_guard<std::mutex> lock(text_mutex_);
        last_text_ = processed;
    }
    fsm_.TriggerEvent(Trigger::SST_COMPLETE);
    if (sink_ != nullptr) {
        sink_->OnStatus("injected " + std::to_string(processed.size()) + " chars");
    }
    std::cout << "[engine] injected " << processed.size() << " chars" << std::endl;
}
