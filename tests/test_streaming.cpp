#include "Fakes.h"

#include "EngineHarness.h"
#include "core/StreamingDictation.h"

#include <doctest/doctest.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace lwf_test;

class ScriptedSegmentStt final : public IInferenceProvider {
public:
    std::vector<std::string> replies;
    std::vector<int> sleep_ms;
    std::vector<std::string> prompts;
    int fail_at = -1;
    int calls = 0;
    std::atomic<bool> cancel_requested{false};

    std::string Transcribe(const std::vector<float>&) override {
        const int index = calls++;
        if (index < static_cast<int>(sleep_ms.size()) && sleep_ms[index] > 0) {
            int remaining = sleep_ms[index];
            while (remaining > 0) {
                if (cancel_requested.load()) {
                    throw std::runtime_error("segment cancelled");
                }
                const int slice = remaining < 10 ? remaining : 10;
                std::this_thread::sleep_for(std::chrono::milliseconds(slice));
                remaining -= slice;
            }
        }
        if (index == fail_at) {
            throw std::runtime_error("segment failed");
        }
        return index < static_cast<int>(replies.size()) ? replies[index] : std::string();
    }

    std::string TranscribeWithPrompt(const std::vector<float>& pcm,
                                     const std::string& prompt) override {
        prompts.push_back(prompt);
        return Transcribe(pcm);
    }

    std::string ProcessText(const std::string& text, const std::string&) override { return text; }

    void RequestCancel() override { cancel_requested.store(true); }
};

TEST_CASE("join texts handles empty sides punctuation and spacing") {
    CHECK(StreamingDictator::JoinTexts("", "") == "");
    CHECK(StreamingDictator::JoinTexts("hello", "") == "hello");
    CHECK(StreamingDictator::JoinTexts("", "world") == "world");
    CHECK(StreamingDictator::JoinTexts("hello", "world") == "hello world");
    CHECK(StreamingDictator::JoinTexts("  hello  ", "  world  ") == "hello world");
    CHECK(StreamingDictator::JoinTexts("hello", ".") == "hello.");
    CHECK(StreamingDictator::JoinTexts("is it here", "?") == "is it here?");
    CHECK(StreamingDictator::JoinTexts("wait", ", then go") == "wait, then go");
    CHECK(StreamingDictator::JoinTexts("hello   ", "world") == "hello world");
}

TEST_CASE("truncate context keeps the tail and caps length") {
    CHECK(StreamingDictator::TruncateContext("") == "");
    CHECK(StreamingDictator::TruncateContext("short") == "short");
    const std::string long_text = "word0 word1 word2 word3 word4 word5 word6 word7";
    const std::string cut = StreamingDictator::TruncateContext(long_text, 20);
    CHECK(cut.size() <= 20);
    CHECK(cut.find("word0") == std::string::npos);
    CHECK(long_text.find(cut) != std::string::npos);
    CHECK(StreamingDictator::TruncateContext("exactlytwentychars!", 20) == "exactlytwentychars!");
}

TEST_CASE("segments are transcribed in strict fifo order despite reversed delays") {
    ScriptedSegmentStt stt;
    stt.replies = {"alpha", "beta", "gamma"};
    stt.sleep_ms = {150, 80, 0};

    StreamingDictator dictator(stt);
    dictator.Begin();
    dictator.CommitSegment(std::vector<float>(512, 0.1f));
    dictator.CommitSegment(std::vector<float>(512, 0.2f));
    dictator.CommitSegment(std::vector<float>(512, 0.3f));
    std::string out;
    const bool ok = dictator.Finish({}, out);
    CHECK(ok);
    CHECK(out == "alpha beta gamma");
    CHECK(dictator.CommittedCount() == 3);
}

TEST_CASE("context prompt carries the previous segment tail") {
    ScriptedSegmentStt stt;
    stt.replies = {"first segment result words", "second segment result"};

    StreamingDictator dictator(stt);
    dictator.Begin();
    dictator.CommitSegment(std::vector<float>(512, 0.1f));
    dictator.CommitSegment(std::vector<float>(512, 0.2f));
    std::vector<float> tail(512, 0.3f);
    std::string out;
    CHECK(dictator.Finish(tail, out));
    REQUIRE(stt.prompts.size() == 3);
    CHECK(stt.prompts[0].empty());
    CHECK(stt.prompts[1] == "first segment result words");
    CHECK(stt.prompts[2] == "second segment result");
    CHECK(out == "first segment result words second segment result");
}

TEST_CASE("segment failure makes finish fail so the engine can fall back") {
    ScriptedSegmentStt stt;
    stt.replies = {"alpha", "beta"};
    stt.fail_at = 1;

    StreamingDictator dictator(stt);
    dictator.Begin();
    dictator.CommitSegment(std::vector<float>(512, 0.1f));
    dictator.CommitSegment(std::vector<float>(512, 0.2f));
    std::string out;
    CHECK_FALSE(dictator.Finish({}, out));
}

TEST_CASE("cancel discards in-flight work and finish returns false quickly") {
    ScriptedSegmentStt stt;
    stt.replies = {"slow"};
    stt.sleep_ms = {400};

    StreamingDictator dictator(stt);
    dictator.Begin();
    dictator.CommitSegment(std::vector<float>(512, 0.1f));
    const auto begin = std::chrono::steady_clock::now();
    dictator.Cancel();
    std::string out;
    CHECK_FALSE(dictator.Finish({}, out));
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 300);
}

TEST_CASE("begin resets state between sessions") {
    ScriptedSegmentStt stt;
    stt.replies = {"one", "two"};

    StreamingDictator dictator(stt);
    dictator.Begin();
    dictator.CommitSegment(std::vector<float>(512, 0.1f));
    std::string out;
    REQUIRE(dictator.Finish({}, out));
    CHECK(out == "one");

    dictator.Begin();
    CHECK(dictator.CommittedCount() == 0);
    std::string out2;
    REQUIRE(dictator.Finish({}, out2));
    CHECK(out2.empty());
}

TEST_CASE("long utterance with a pause commits a segment and joins with the tail") {
    Harness h;
    h.stt.transcript = "hello world";
    h.settings.streaming_stt = true;
    h.settings.streaming_min_segment_ms = 3000;
    h.settings.streaming_pause_ms = 700;
    auto engine = MakeEngine(h);
    engine->Start();

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    CHECK(engine->StreamedSegments() == 1);
    h.vad.SetSpeech(true);
    for (int i = 0; i < 20; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    for (int i = 0; i < 4; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    REQUIRE_FALSE(h.injector.injected.empty());
    CHECK(h.injector.injected.back() == "hello world hello world");
    CHECK(h.stt.transcribe_count == 2);
    CHECK(h.stt.prompts.size() == 2);
    CHECK(h.stt.prompts[0].empty());
    CHECK(h.stt.prompts[1] == "hello world");
    engine->Stop();
}

TEST_CASE("short utterance never streams and runs a single one-shot transcription") {
    Harness h;
    h.settings.streaming_stt = true;
    auto engine = MakeEngine(h);
    engine->Start();
    PressAndSpeak(h, *engine);
    CHECK(engine->StreamedSegments() == 0);
    CHECK(h.stt.transcribe_count == 1);
    REQUIRE_FALSE(h.injector.injected.empty());
    engine->Stop();
}

TEST_CASE("pause shorter than the configured threshold does not commit a segment") {
    Harness h;
    h.settings.streaming_stt = true;
    h.settings.streaming_min_segment_ms = 3000;
    h.settings.streaming_pause_ms = 700;
    auto engine = MakeEngine(h);
    engine->Start();

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 12; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(true);
    for (int i = 0; i < 10; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    for (int i = 0; i < 4; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(engine->StreamedSegments() == 0);
    CHECK(h.stt.transcribe_count == 1);
    engine->Stop();
}

TEST_CASE("streaming disabled keeps the one-shot path") {
    Harness h;
    h.settings.streaming_stt = false;
    auto engine = MakeEngine(h);
    engine->Start();

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(engine->StreamedSegments() == 0);
    CHECK(h.stt.transcribe_count == 1);
    REQUIRE_FALSE(h.injector.injected.empty());
    engine->Stop();
}

TEST_CASE("segment failure falls back to one-shot transcription of the full buffer") {
    FakeClock clock;
    FakeAudioCapture capture;
    ScriptedVad vad{false};
    ScriptedSegmentStt stt;
    stt.replies = {"partial one", "partial two", "one-shot full text"};
    stt.fail_at = 1;
    PassthroughLlm llm;
    DictationCleanup cleanup(&llm);
    TextPipeline pipeline(cleanup);
    RecordingInjector injector;
    DictationFSM fsm;
    RecordingSink sink;
    EngineSettings settings;
    settings.streaming_stt = true;
    settings.streaming_min_segment_ms = 3000;
    settings.streaming_pause_ms = 700;

    DictationEngine engine(capture, &vad, stt, pipeline, injector, fsm, clock, sink, settings);
    engine.Start();
    engine.PostEvent(EngineEventType::PttPress);
    engine.ProcessOne(0);
    vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        capture.Append(voiced);
        engine.ProcessOne(0);
    }
    vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        capture.Append(silent);
        engine.ProcessOne(0);
    }
    vad.SetSpeech(true);
    for (int i = 0; i < 100; ++i) {
        capture.Append(voiced);
        engine.ProcessOne(0);
    }
    vad.SetSpeech(false);
    for (int i = 0; i < 26; ++i) {
        capture.Append(silent);
        engine.ProcessOne(0);
    }
    vad.SetSpeech(true);
    for (int i = 0; i < 20; ++i) {
        capture.Append(voiced);
        engine.ProcessOne(0);
    }
    vad.SetSpeech(false);
    for (int i = 0; i < 4; ++i) {
        capture.Append(silent);
        engine.ProcessOne(0);
    }
    engine.PostEvent(EngineEventType::PttRelease);
    engine.ProcessOne(0);

    CHECK(engine.StreamedSegments() == 2);
    CHECK(stt.calls == 3);
    REQUIRE_FALSE(injector.injected.empty());
    CHECK(injector.injected.back() == "one-shot full text");
    CHECK(fsm.GetCurrentState() == State::IDLE);
    engine.Stop();
}

TEST_CASE("cancel during streaming discards partials and injects nothing") {
    Harness h;
    h.settings.streaming_stt = true;
    h.settings.streaming_min_segment_ms = 3000;
    h.settings.streaming_pause_ms = 700;
    h.stt.on_transcribe = [] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    };
    auto engine = MakeEngine(h);
    engine->Start();

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.Append(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        h.capture.Append(silent);
        engine->ProcessOne(0);
    }
    CHECK(engine->StreamedSegments() == 1);
    engine->PostEvent(EngineEventType::Cancel);
    engine->ProcessOne(0);
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.injector.injected.empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "cancelled"));
    engine->Stop();
}

}
