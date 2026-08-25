#pragma once

#include <mutex>
#include <string>

class ConfigManager {
public:
    explicit ConfigManager(std::string path = "config.json");

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void Reload();

    int HotkeyCode();
    std::string WhisperModelPath();
    float VadThreshold();
    std::string LlmModelPath();
    bool UseRemote();
    std::string RemoteEndpoint();

private:
    struct Data {
        int hotkey_code = 0;
        std::string whisper_model_path;
        float vad_threshold = 0.0f;
        std::string llm_model_path;
        bool use_remote = false;
        std::string remote_endpoint = "localhost:50051";
    };

    Data ParseFile(const std::string& path) const;

    std::string path_;
    std::mutex mutex_;
    Data data_;
};
