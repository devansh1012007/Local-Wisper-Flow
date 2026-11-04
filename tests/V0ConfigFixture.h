#pragma once

#include <filesystem>
#include <fstream>
#include <string>

// Seeds tests/data/config_v0.json when it is missing. The v0 body mirrors the
// pre-schema config format that the backward-compat gate (G5) must keep
// loading; the file used to be untracked on the old dev box, which made this
// gate fail on fresh clones (phase 0 report, BASE_DECISION section 4).
inline const std::string& SeedV0Config() {
    static const std::string body = R"({
  "hotkey_code": 165,
  "whisper_model_path": "ggml-base.en.bin",
  "vad_threshold": 0.6,
  "llm_model_path": "models/Phi-4-mini-reasoning-GGUF/Phi-4-mini-reasoning-Q8_0.gguf",
  "use_remote": false,
  "remote_endpoint": "localhost:50051"
}
)";
    const std::string path = "tests/data/config_v0.json";
    std::error_code ec;
    std::filesystem::create_directories("tests/data", ec);
    std::ifstream existing(path);
    if (!existing.good()) {
        existing.close();
        std::ofstream out(path);
        out << body;
    }
    return body;
}

// Seeds the root-level data files the suite reads from the repo working
// directory. Like config_v0.json these were untracked on the old dev box and
// vanished with it; seeding them keeps the suite hermetic on fresh clones.
inline void SeedTestDataFiles() {
    SeedV0Config();
    {
        std::ifstream existing("transforms.json");
        if (!existing.good()) {
            existing.close();
            std::ofstream out("transforms.json");
            out << R"json([
  {"name": "Fix Grammar",
   "prompt": "Fix the grammar of the following text without changing its meaning:\n\n{text}"}
])json";
        }
    }
    {
        std::ifstream existing("config.json");
        if (!existing.good()) {
            existing.close();
            std::ofstream out("config.json");
            out << R"json({
  "schema_version": 1,
  "whisper_model_path": "ggml-base.en.bin",
  "vad_threshold": 0.6
})json";
        }
    }
}
