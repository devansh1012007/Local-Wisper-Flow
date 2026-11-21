#pragma once

#include "Fakes.h"

#include "core/DictationCleanup.h"
#include "core/DictationEngine.h"
#include "core/DictationFSM.h"
#include "core/HotkeyBinding.h"
#include "core/HotkeyTracker.h"
#include "core/TextPipeline.h"

#include <memory>
#include <string>
#include <vector>

namespace lwf_test {

constexpr int kChunkSamples = 512;

struct Harness {
    FakeClock clock;
    FakeAudioCapture capture;
    ScriptedVad vad{false};
    FakeStt stt;
    PassthroughLlm llm;
    DictationCleanup cleanup{&llm};
    TextPipeline pipeline{cleanup};
    RecordingInjector injector;
    DictationFSM fsm;
    RecordingSink sink;
    EngineSettings settings;

    void Speak(int chunks = 4) {
        vad.SetSpeech(true);
        const std::vector<float> chunk(kChunkSamples, 0.3f);
        for (int i = 0; i < chunks; ++i) {
            clock.Advance(30);
            capture.Append(chunk);
        }
    }

    void Silence(int chunks = 2) {
        vad.SetSpeech(false);
        const std::vector<float> chunk(kChunkSamples, 0.0f);
        for (int i = 0; i < chunks; ++i) {
            clock.Advance(30);
            capture.Append(chunk);
        }
    }
};

inline std::unique_ptr<DictationEngine> MakeEngine(Harness& h) {
    return std::make_unique<DictationEngine>(h.capture, &h.vad, h.stt, h.pipeline,
                                                  h.injector, h.fsm, h.clock, h.sink, h.settings);
}

inline void PressAndSpeak(Harness& h, DictationEngine& engine) {
    engine.PostEvent(EngineEventType::PttPress);
    engine.ProcessOne(0);
    h.Speak();
    engine.PostEvent(EngineEventType::PttRelease);
    engine.ProcessOne(0);
}

inline bool HasStatus(const std::vector<std::string>& statuses, const std::string& needle) {
    for (const std::string& status : statuses) {
        if (status.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}

