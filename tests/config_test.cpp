#include <doctest/doctest.h>

#include "core/ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

const char* kConfigPath = "tests/data/config_test.json";

void WriteConfig(const std::string& body) {
    std::filesystem::create_directories("tests/data");
    std::ofstream file(kConfigPath);
    file << body;
}

}  // namespace

TEST_CASE("config v0 keys still load") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6,
        "llm_model_path": "models/some-model.gguf",
        "use_remote": false,
        "remote_endpoint": "localhost:50051"
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.HotkeyCode() == 165);
    CHECK(config.WhisperModelPath() == "ggml-base.en.bin");
    CHECK(config.VadThreshold() == 0.6f);
    CHECK(config.LlmModelPath() == "models/some-model.gguf");
    CHECK(config.UseRemote() == false);
    CHECK(config.RemoteEndpoint() == "localhost:50051");
}

TEST_CASE("config defaults apply for absent optional keys") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.CleanupEnabled());
    CHECK(config.CleanupMinWords() == 4);
    CHECK(config.CleanupTimeoutMs() == 4000);
    CHECK(config.MinHoldMs() == 100);
    CHECK(config.HandsFreeSilenceMs() == 0);
    CHECK(config.MaxRecordingMs() == 30000);
    CHECK(config.LlmContextSize() == 2048);
    CHECK(config.LlmMaxPredictTokens() == 1024);
    CHECK(config.LlmGpuLayers() == 0);
    CHECK(config.VadModelPath() == "models/silero_vad.onnx");
}

TEST_CASE("config new keys load when present") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6,
        "cleanup_enabled": false,
        "cleanup_min_words": 7,
        "cleanup_timeout_ms": 8000,
        "min_hold_ms": 250,
        "hands_free_silence_ms": 1200,
        "max_recording_ms": 60000,
        "llm_context_size": 4096,
        "llm_max_predict_tokens": 2048,
        "llm_gpu_layers": 99,
        "vad_model_path": "models/custom.onnx"
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.CleanupEnabled() == false);
    CHECK(config.CleanupMinWords() == 7);
    CHECK(config.CleanupTimeoutMs() == 8000);
    CHECK(config.MinHoldMs() == 250);
    CHECK(config.HandsFreeSilenceMs() == 1200);
    CHECK(config.MaxRecordingMs() == 60000);
    CHECK(config.LlmContextSize() == 4096);
    CHECK(config.LlmMaxPredictTokens() == 2048);
    CHECK(config.LlmGpuLayers() == 99);
    CHECK(config.VadModelPath() == "models/custom.onnx");
}

TEST_CASE("config tolerates optional keys of the wrong type") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6,
        "cleanup_min_words": "4",
        "use_remote": "yes",
        "llm_gpu_layers": 12.5
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.CleanupMinWords() == 4);
    CHECK(config.UseRemote() == false);
    CHECK(config.LlmGpuLayers() == 0);
    CHECK(config.Warnings().size() == 3);
}

TEST_CASE("config out of range values are clamped with warnings") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6,
        "vad_threshold_dup": 0,
        "cleanup_min_words": 500,
        "cleanup_timeout_ms": 10,
        "min_hold_ms": -50,
        "hands_free_silence_ms": -700,
        "max_recording_ms": 10,
        "llm_context_size": 8,
        "llm_max_predict_tokens": 99999,
        "inject_restore_delay_ms": 1,
        "whisper_threads": 64,
        "whisper_beam_size": 0
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.CleanupMinWords() == 50);
    CHECK(config.CleanupTimeoutMs() == 100);
    CHECK(config.MinHoldMs() == 0);
    CHECK(config.HandsFreeSilenceMs() == 0);
    CHECK(config.MaxRecordingMs() == 1000);
    CHECK(config.LlmContextSize() == 512);
    CHECK(config.LlmMaxPredictTokens() == 8192);
    CHECK(config.InjectRestoreDelayMs() == 50);
    CHECK(config.WhisperThreads() == 16);
    CHECK(config.WhisperBeamSize() == 1);
    CHECK(config.Warnings().size() >= 8);
}

TEST_CASE("config whisper and hotkey keys load with defaults") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 0.6
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.WhisperLanguage() == "auto");
    CHECK(config.WhisperThreads() == 4);
    CHECK(config.WhisperBeamSize() == 1);
    CHECK(config.WhisperDisableTempFallback() == false);
    CHECK(config.AvoidAltGr() == true);
    CHECK(config.DictionaryPath() == "dictionary.json");
}

TEST_CASE("config vad threshold out of range is clamped") {
    WriteConfig(R"({
        "hotkey_code": 165,
        "whisper_model_path": "ggml-base.en.bin",
        "vad_threshold": 5.0
    })");
    ConfigManager config(kConfigPath);
    CHECK(config.VadThreshold() == doctest::Approx(0.99f));
}

TEST_CASE("config missing required key fails loudly") {
    WriteConfig(R"({
        "hotkey_code": 165
    })");
    CHECK_THROWS_AS([&] { ConfigManager config(kConfigPath); }(), std::runtime_error);
}
