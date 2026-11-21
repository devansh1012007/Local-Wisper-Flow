#include "Fakes.h"

#include "EngineHarness.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace lwf_test;

TEST_CASE("audio pcm leaves no file trace in the working directory during dictations") {
    const std::filesystem::path base = std::filesystem::temp_directory_path() / "lwf_no_audio_dump";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const std::filesystem::path original = std::filesystem::current_path();
    std::filesystem::current_path(base);

    {
        Harness h;
        h.settings.streaming_stt = true;
        auto engine = MakeEngine(h);
        engine->Start();

        for (int round = 0; round < 3; ++round) {
            engine->PostEvent(EngineEventType::PttPress);
            engine->ProcessOne(0);
            h.vad.SetSpeech(true);
            const std::vector<float> voiced(kChunkSamples, 0.3f);
            for (int i = 0; i < 40; ++i) {
                h.capture.Append(voiced);
                engine->ProcessOne(0);
            }
            h.vad.SetSpeech(false);
            const std::vector<float> silent(kChunkSamples, 0.0f);
            for (int i = 0; i < 4; ++i) {
                h.capture.Append(silent);
                engine->ProcessOne(0);
            }
            engine->PostEvent(EngineEventType::PttRelease);
            engine->ProcessOne(0);
            engine->PostEvent(EngineEventType::Cancel);
            engine->ProcessOne(0);
        }
        engine->Stop();
    }

    std::error_code ec;
    std::filesystem::directory_iterator it(base, ec);
    int files_created = 0;
    if (!ec) {
        for (const std::filesystem::directory_entry& entry : it) {
            if (entry.is_regular_file(ec)) {
                ++files_created;
            }
        }
    }

    std::filesystem::current_path(original);
    std::filesystem::remove_all(base);
    CHECK(files_created == 0);
}

}
