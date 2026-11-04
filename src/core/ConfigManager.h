#pragma once

#include "core/SettingsStore.h"

#include <memory>
#include <mutex>
#include <string>

class ConfigManager {
public:
    explicit ConfigManager(std::string path = "config.json");

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void Reload();
    void Save();

    int HotkeyCode();
    std::string WhisperModelPath();
    float VadThreshold();
    std::string LlmModelPath();
    bool UseRemote();
    std::string RemoteEndpoint();
    std::string SttProvider();
    bool SttConsentAccepted();
    std::string SttLanguage();
    int SttTimeoutMs();
    int SttMaxClips();

    void SetHotkeyCode(int value);
    void SetWhisperModelPath(const std::string& value);
    void SetVadThreshold(float value);
    void SetLlmModelPath(const std::string& value);
    void SetUseRemote(bool value);
    void SetRemoteEndpoint(const std::string& value);

    SettingsStore& Store();

private:
    std::unique_ptr<SettingsStore> store_;
    std::mutex mutex_;
};
