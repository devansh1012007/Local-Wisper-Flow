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

// Test-side harness for the engine integration tests. The fakes themselves live
// in Fakes.h (global namespace); this header wires them together.
constexpr int kChunkSamples = 512;

struct Harness {
    FakeClock clock;
    FakeAudioCapture capture;
    ScriptedVad vad{std::vector<bool>{}};
    FakeStt stt;
    PassthroughLlm llm;
    DictationCleanup cleanup{&llm};
    TextPipeline pipeline{cleanup};
    RecordingInjector injector;
    DictationFSM fsm;
    RecordingSink sink;
    EngineSettings settings;

    Harness() {
        // Historical harness default: a two-word transcript exercises the plain
        // record->transcribe->inject path without tripping the cleanup word
        // minimum. Tests that want more call MakeEngine with their own settings.
        stt.transcript = "hello world";
    }

    void Speak(int chunks = 4) {
        vad.SetSpeech(true);
        const std::vector<float> chunk(kChunkSamples, 0.3f);
        for (int i = 0; i < chunks; ++i) {
            clock.Advance(30);
            capture.FeedSamples(chunk);
        }
    }

    void Silence(int chunks = 2) {
        vad.SetSpeech(false);
        const std::vector<float> chunk(kChunkSamples, 0.0f);
        for (int i = 0; i < chunks; ++i) {
            clock.Advance(30);
            capture.FeedSamples(chunk);
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
