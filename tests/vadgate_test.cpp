#include <doctest/doctest.h>

#include "core/VadGate.h"

#include <cstddef>
#include <vector>

#include "fakes.h"

TEST_CASE("vad gate feeds exactly 512 sample chunks") {
    ScriptedVad vad({true, false, true});
    VadGate gate(&vad);

    gate.Feed(MakeSpeech(700).data(), 700);
    gate.Feed(MakeSpeech(400).data(), 400);

    REQUIRE(vad.chunk_sizes.size() == 2);
    CHECK(vad.chunk_sizes[0] == 512);
    CHECK(vad.chunk_sizes[1] == 512);
}

TEST_CASE("vad gate reset clears state and notifies the provider") {
    ScriptedVad vad({true});
    VadGate gate(&vad);
    gate.Feed(MakeSpeech(512).data(), 512);
    REQUIRE(gate.SpeechDetected());

    gate.Reset();
    CHECK(vad.reset_count.load() == 1);
    CHECK_FALSE(gate.SpeechDetected());
    CHECK(gate.TotalSamples() == 0);
}

TEST_CASE("vad gate tracks silence since the last speech chunk") {
    ScriptedVad vad({true, false, false, false});
    VadGate gate(&vad);

    gate.Feed(MakeSpeech(512).data(), 512);
    CHECK(gate.SilenceMs(16000).count() == 0);

    gate.Feed(MakeSilence(512).data(), 512);
    gate.Feed(MakeSilence(512).data(), 512);
    gate.Feed(MakeSilence(512).data(), 512);
    CHECK(gate.SilenceMs(16000).count() == 96);
}

TEST_CASE("vad gate speech interval pads head and tail") {
    ScriptedVad vad({false, false, true, false, false});
    VadGate gate(&vad);

    const std::size_t total = 512 * 5;
    for (int i = 0; i < 5; ++i) {
        gate.Feed(MakeSpeech(512).data(), 512);
    }

    const std::pair<std::size_t, std::size_t> interval = gate.SpeechInterval(total);
    CHECK(interval.first == 0);
    CHECK(interval.second == total);
}

TEST_CASE("vad gate speech interval does not exceed the recording") {
    ScriptedVad vad({true});
    VadGate gate(&vad);

    const std::size_t total = 512;
    gate.Feed(MakeSpeech(512).data(), 512);

    const std::pair<std::size_t, std::size_t> interval = gate.SpeechInterval(total);
    CHECK(interval.first == 0);
    CHECK(interval.second == 512);
}

TEST_CASE("vad gate without a provider falls back to an energy floor") {
    VadGate gate(nullptr);
    gate.Feed(MakeSilence(1024).data(), 1024);
    CHECK_FALSE(gate.SpeechDetected());

    gate.Reset();
    gate.Feed(MakeSpeech(1024).data(), 1024);
    CHECK(gate.SpeechDetected());
}

TEST_CASE("vad gate duration tracks total samples fed") {
    ScriptedVad vad({true});
    VadGate gate(&vad);
    gate.Feed(MakeSpeech(16000).data(), 16000);
    CHECK(gate.DurationMs(16000).count() >= 990);
}
