#include "core/ConfigManager.h"

#include <doctest/doctest.h>

TEST_CASE("v0 config loads unchanged (backward compat gate)") {
    ConfigManager config("tests/data/config_v0.json");
    CHECK(config.HotkeyCode() == 165);
    CHECK(config.WhisperModelPath() == "ggml-base.en.bin");
    CHECK(config.VadThreshold() == doctest::Approx(0.6f));
    CHECK(config.LlmModelPath() ==
          "models/Phi-4-mini-reasoning-GGUF/Phi-4-mini-reasoning-Q8_0.gguf");
    CHECK_FALSE(config.UseRemote());
    CHECK(config.RemoteEndpoint() == "localhost:50051");
}

TEST_CASE("repo config.json loads") {
    ConfigManager config("config.json");
    CHECK(config.VadThreshold() > 0.0f);
    CHECK_FALSE(config.WhisperModelPath().empty());
}
