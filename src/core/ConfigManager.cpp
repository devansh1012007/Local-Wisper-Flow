#include "core/ConfigManager.h"

ConfigManager::ConfigManager(std::string path)
    : store_(std::make_unique<SettingsStore>(path.empty() ? std::string("config.json") : path)) {}

void ConfigManager::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    store_->Load();
}

void ConfigManager::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    store_->Save();
}

int ConfigManager::HotkeyCode() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetInt("hotkey_code");
}

std::string ConfigManager::WhisperModelPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetString("whisper_model_path");
}

float ConfigManager::VadThreshold() {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<float>(store_->GetDouble("vad_threshold"));
}

std::string ConfigManager::LlmModelPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetString("llm_model_path");
}

bool ConfigManager::UseRemote() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetBool("use_remote");
}

std::string ConfigManager::RemoteEndpoint() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetString("remote_endpoint");
}

void ConfigManager::SetHotkeyCode(int value) {
    store_->SetInt("hotkey_code", value);
}

void ConfigManager::SetWhisperModelPath(const std::string& value) {
    store_->SetString("whisper_model_path", value);
}

void ConfigManager::SetVadThreshold(float value) {
    store_->SetDouble("vad_threshold", static_cast<double>(value));
}

void ConfigManager::SetLlmModelPath(const std::string& value) {
    store_->SetString("llm_model_path", value);
}

void ConfigManager::SetUseRemote(bool value) {
    store_->SetBool("use_remote", value);
}

void ConfigManager::SetRemoteEndpoint(const std::string& value) {
    store_->SetString("remote_endpoint", value);
}

SettingsStore& ConfigManager::Store() {
    return *store_;
}

std::string ConfigManager::SttProvider() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetString("stt_provider");
}

bool ConfigManager::SttConsentAccepted() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetBool("stt_consent_accepted");
}

std::string ConfigManager::SttLanguage() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetString("stt_language");
}

int ConfigManager::SttTimeoutMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetInt("stt_timeout_ms");
}

int ConfigManager::SttMaxClips() {
    std::lock_guard<std::mutex> lock(mutex_);
    return store_->GetInt("stt_max_clips");
}
