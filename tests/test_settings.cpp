#include "core/ConfigManager.h"
#include "core/PathResolver.h"
#include "os/DataDir.h"

#include <doctest/doctest.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string TempDir(const char* name) {
    const std::string dir = (std::filesystem::path("/tmp") / (std::string("lwf_test_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

}

TEST_CASE("settings round trip preserves known and unknown keys") {
    const std::string dir = TempDir("roundtrip");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, R"({"hotkey_code": 33, "vendor_future_key": {"nested": [1,2]}})");

    {
        SettingsStore store(cfg);
        CHECK(store.GetInt("hotkey_code") == 33);
        store.SetInt("min_hold_ms", 400);
        store.SetString("whisper_model_path", "ggml-tiny.bin");
        store.Save();
    }
    {
        SettingsStore store(cfg);
        CHECK(store.GetInt("min_hold_ms") == 400);
        CHECK(store.GetString("whisper_model_path") == "ggml-tiny.bin");
        CHECK(store.GetInt("hotkey_code") == 33);
    }
    const std::string saved = ReadFile(cfg);
    CHECK(saved.find("vendor_future_key") != std::string::npos);
    CHECK(saved.find("hotkey_code") != std::string::npos);
}

TEST_CASE("v0 config loads with defaults for new keys") {
    SettingsStore store("tests/data/config_v0.json");
    CHECK(store.GetInt("hotkey_code") == 165);
    CHECK(store.GetString("whisper_model_path") == "ggml-base.en.bin");
    CHECK(store.GetDouble("vad_threshold") == doctest::Approx(0.6));
    CHECK(store.GetBool("use_remote") == false);
    CHECK(store.GetInt("min_hold_ms") == 250);
    CHECK(store.GetString("hotkey_cancel") == "Escape");
    CHECK_FALSE(store.CorruptOnLastLoad());
}

TEST_CASE("out of range values are clamped and reported") {
    const std::string dir = TempDir("clamp");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, R"({"vad_threshold": 7.5, "min_hold_ms": -100})");

    SettingsStore store(cfg);
    CHECK(store.GetDouble("vad_threshold") == doctest::Approx(1.0));
    CHECK(store.GetInt("min_hold_ms") == 50);
    const std::vector<std::string> clamped = store.ClampedOnLastLoad();
    REQUIRE(clamped.size() == 2);
}

TEST_CASE("corrupt config is renamed to .bad and defaults load") {
    const std::string dir = TempDir("corrupt");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, "{ this is not json !!!");

    SettingsStore store(cfg);
    CHECK(store.CorruptOnLastLoad());
    CHECK(std::filesystem::exists(cfg + ".bad"));
    CHECK(store.GetInt("min_hold_ms") == 250);
}

TEST_CASE("stale tmp file does not break loading and save keeps a backup") {
    const std::string dir = TempDir("powerloss");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, R"({"hotkey_code": 10})");

    SettingsStore store(cfg);
    CHECK(store.GetInt("hotkey_code") == 10);
    store.SetInt("hotkey_code", 11);
    store.Save();
    WriteFile(cfg + ".tmp", "garbage from a crash");
    store.SetInt("hotkey_code", 12);
    store.Save();

    SettingsStore reloaded(cfg);
    CHECK(reloaded.GetInt("hotkey_code") == 12);
    CHECK(std::filesystem::exists(cfg + ".bak"));
}

TEST_CASE("concurrent get and set from eight threads is safe") {
    const std::string dir = TempDir("threads");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, "{}");

    SettingsStore store(cfg);
    std::vector<std::thread> threads;
    std::atomic<bool> mismatch{false};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&store, t, &mismatch] {
            for (int i = 0; i < 200; ++i) {
                store.SetInt("min_hold_ms", 100 + ((t + i) % 400));
                const int value = store.GetInt("min_hold_ms");
                if (value < 50 || value > 5000) {
                    mismatch.store(true);
                }
                (void)store.GetBool("sound_enabled");
                (void)store.GetString("remote_endpoint");
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    CHECK_FALSE(mismatch.load());
}

TEST_CASE("config manager delegates and exposes setters") {
    const std::string dir = TempDir("cm");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    WriteFile(cfg, R"({"hotkey_code": 165, "whisper_model_path": "ggml-base.en.bin", "vad_threshold": 0.6})");

    ConfigManager config(cfg);
    CHECK(config.HotkeyCode() == 165);
    config.SetVadThreshold(0.9f);
    config.SetUseRemote(true);
    config.SetRemoteEndpoint("127.0.0.1:1234");
    config.Save();
    config.Reload();

    CHECK(config.VadThreshold() == doctest::Approx(0.9f));
    CHECK(config.UseRemote());
    CHECK(config.RemoteEndpoint() == "127.0.0.1:1234");
    CHECK(config.WhisperModelPath() == "ggml-base.en.bin");
}

TEST_CASE("path resolver prefers data dir then exe dir then as-is") {
    const std::string dir = TempDir("resolver");
    std::filesystem::create_directories((std::filesystem::path(dir) / "data" / "models").string());
    std::filesystem::create_directories((std::filesystem::path(dir) / "exe" / "models").string());
    const std::string in_data = (std::filesystem::path(dir) / "data" / "models" / "m.bin").string();
    const std::string in_exe = (std::filesystem::path(dir) / "exe" / "models" / "m.bin").string();
    WriteFile(in_data, "d");
    WriteFile(in_exe, "e");

    CHECK(ResolveDataPath("/abs/model.bin", dir + "/data", dir + "/exe") == "/abs/model.bin");
    CHECK(ResolveDataPath("models/m.bin", dir + "/data", dir + "/exe") == in_data);
    std::filesystem::remove(in_data);
    CHECK(ResolveDataPath("models/m.bin", dir + "/data", dir + "/exe") == in_exe);
    std::filesystem::remove(in_exe);
    CHECK(ResolveDataPath("models/m.bin", dir + "/data", dir + "/exe") == "models/m.bin");
    CHECK(ResolveDataPath("", dir, dir).empty());
}

TEST_CASE("data dir honors LWF_HOME override") {
    const std::string dir = TempDir("lwfhomedir");
    setenv("LWF_HOME", dir.c_str(), 1);
    CHECK(GetDataDir() == dir);
    unsetenv("LWF_HOME");
}
