#include "core/ConfigManager.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr const char* kHotkeyCodeKey = "hotkey_code";
constexpr const char* kWhisperModelPathKey = "whisper_model_path";
constexpr const char* kVadThresholdKey = "vad_threshold";
constexpr const char* kLlmModelPathKey = "llm_model_path";
constexpr const char* kUseRemoteKey = "use_remote";
constexpr const char* kRemoteEndpointKey = "remote_endpoint";

}  // namespace

ConfigManager::ConfigManager(std::string path)
    : path_(std::move(path)) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = ParseFile(path_);
}

void ConfigManager::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = ParseFile(path_);
}

int ConfigManager::HotkeyCode() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.hotkey_code;
}

std::string ConfigManager::WhisperModelPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.whisper_model_path;
}

float ConfigManager::VadThreshold() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.vad_threshold;
}

std::string ConfigManager::LlmModelPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.llm_model_path;
}

bool ConfigManager::UseRemote() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.use_remote;
}

std::string ConfigManager::RemoteEndpoint() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.remote_endpoint;
}

ConfigManager::Data ConfigManager::ParseFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("ConfigManager: cannot open config file '" + path + "'");
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("ConfigManager: failed to parse '" + path + "': " + e.what());
    }

    Data data;
    try {
        data.hotkey_code = json.at(kHotkeyCodeKey).get<int>();
        data.whisper_model_path = json.at(kWhisperModelPathKey).get<std::string>();
        data.vad_threshold = json.at(kVadThresholdKey).get<float>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("ConfigManager: invalid config: ") + e.what());
    }
    data.llm_model_path = json.value(kLlmModelPathKey, std::string());
    data.use_remote = json.value(kUseRemoteKey, false);
    data.remote_endpoint = json.value(kRemoteEndpointKey, std::string("localhost:50051"));
    return data;
}
