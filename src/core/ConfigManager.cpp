#include "core/ConfigManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr const char* kHotkeyCodeKey = "hotkey_code";
constexpr const char* kWhisperModelPathKey = "whisper_model_path";
constexpr const char* kVadThresholdKey = "vad_threshold";
constexpr const char* kLlmModelPathKey = "llm_model_path";
constexpr const char* kUseRemoteKey = "use_remote";
constexpr const char* kRemoteEndpointKey = "remote_endpoint";
constexpr const char* kCleanupEnabledKey = "cleanup_enabled";
constexpr const char* kCleanupMinWordsKey = "cleanup_min_words";
constexpr const char* kCleanupTimeoutMsKey = "cleanup_timeout_ms";
constexpr const char* kMinHoldMsKey = "min_hold_ms";
constexpr const char* kHandsFreeSilenceMsKey = "hands_free_silence_ms";
constexpr const char* kMaxRecordingMsKey = "max_recording_ms";
constexpr const char* kLlmContextSizeKey = "llm_context_size";
constexpr const char* kLlmMaxPredictTokensKey = "llm_max_predict_tokens";
constexpr const char* kLlmGpuLayersKey = "llm_gpu_layers";
constexpr const char* kVadModelPathKey = "vad_model_path";
constexpr const char* kInjectRestoreDelayMsKey = "inject_restore_delay_ms";
constexpr const char* kWhisperLanguageKey = "whisper_language";
constexpr const char* kWhisperThreadsKey = "whisper_threads";
constexpr const char* kWhisperBeamSizeKey = "whisper_beam_size";
constexpr const char* kWhisperDisableTempFallbackKey = "whisper_disable_temp_fallback";
constexpr const char* kAvoidAltGrKey = "avoid_altgr";
constexpr const char* kDictionaryPathKey = "dictionary_path";

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

bool ConfigManager::CleanupEnabled() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.cleanup_enabled;
}

int ConfigManager::CleanupMinWords() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.cleanup_min_words;
}

int ConfigManager::CleanupTimeoutMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.cleanup_timeout_ms;
}

int ConfigManager::MinHoldMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.min_hold_ms;
}

int ConfigManager::HandsFreeSilenceMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.hands_free_silence_ms;
}

int ConfigManager::MaxRecordingMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.max_recording_ms;
}

int ConfigManager::LlmContextSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.llm_context_size;
}

int ConfigManager::LlmMaxPredictTokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.llm_max_predict_tokens;
}

int ConfigManager::LlmGpuLayers() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.llm_gpu_layers;
}

std::string ConfigManager::VadModelPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.vad_model_path;
}

int ConfigManager::InjectRestoreDelayMs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.inject_restore_delay_ms;
}

std::string ConfigManager::WhisperLanguage() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.whisper_language;
}

int ConfigManager::WhisperThreads() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.whisper_threads;
}

int ConfigManager::WhisperBeamSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.whisper_beam_size;
}

bool ConfigManager::WhisperDisableTempFallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.whisper_disable_temp_fallback;
}

bool ConfigManager::AvoidAltGr() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.avoid_altgr;
}

std::string ConfigManager::DictionaryPath() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.dictionary_path;
}

const std::vector<std::string>& ConfigManager::Warnings() const {
    return warnings_;
}

ConfigManager::Data ConfigManager::ParseFile(const std::string& path) {
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

    const auto read_string = [&](const char* key, std::string& target) {
        const auto it = json.find(key);
        if (it == json.end()) {
            return;
        }
        if (it->is_string()) {
            target = it->get<std::string>();
        } else {
            warnings_.push_back(std::string("ConfigManager: '") + key +
                                "' has the wrong type, using default");
        }
    };
    const auto read_bool = [&](const char* key, bool& target) {
        const auto it = json.find(key);
        if (it == json.end()) {
            return;
        }
        if (it->is_boolean()) {
            target = it->get<bool>();
        } else {
            warnings_.push_back(std::string("ConfigManager: '") + key +
                                "' has the wrong type, using default");
        }
    };
    const auto read_int = [&](const char* key, int& target) {
        const auto it = json.find(key);
        if (it == json.end()) {
            return;
        }
        if (it->is_number_integer()) {
            target = it->get<int>();
        } else {
            warnings_.push_back(std::string("ConfigManager: '") + key +
                                "' has the wrong type, using default");
        }
    };
    const auto clamp_int = [&](const char* key, int& target, int low, int high) {
        if (target < low) {
            warnings_.push_back(std::string("ConfigManager: '") + key + "' below " +
                                std::to_string(low) + ", clamped");
            target = low;
        } else if (target > high) {
            warnings_.push_back(std::string("ConfigManager: '") + key + "' above " +
                                std::to_string(high) + ", clamped");
            target = high;
        }
    };

    read_string(kLlmModelPathKey, data.llm_model_path);
    read_bool(kUseRemoteKey, data.use_remote);
    read_string(kRemoteEndpointKey, data.remote_endpoint);
    read_bool(kCleanupEnabledKey, data.cleanup_enabled);
    read_int(kCleanupMinWordsKey, data.cleanup_min_words);
    read_int(kCleanupTimeoutMsKey, data.cleanup_timeout_ms);
    read_int(kMinHoldMsKey, data.min_hold_ms);
    read_int(kHandsFreeSilenceMsKey, data.hands_free_silence_ms);
    read_int(kMaxRecordingMsKey, data.max_recording_ms);
    read_int(kLlmContextSizeKey, data.llm_context_size);
    read_int(kLlmMaxPredictTokensKey, data.llm_max_predict_tokens);
    read_int(kLlmGpuLayersKey, data.llm_gpu_layers);
    read_string(kVadModelPathKey, data.vad_model_path);
    read_int(kInjectRestoreDelayMsKey, data.inject_restore_delay_ms);
    read_string(kWhisperLanguageKey, data.whisper_language);
    read_int(kWhisperThreadsKey, data.whisper_threads);
    read_int(kWhisperBeamSizeKey, data.whisper_beam_size);
    read_bool(kWhisperDisableTempFallbackKey, data.whisper_disable_temp_fallback);
    read_bool(kAvoidAltGrKey, data.avoid_altgr);
    read_string(kDictionaryPathKey, data.dictionary_path);

    if (data.vad_threshold < 0.01f || data.vad_threshold > 0.99f) {
        warnings_.push_back("ConfigManager: 'vad_threshold' outside [0.01, 0.99], clamped");
        data.vad_threshold = std::min(0.99f, std::max(0.01f, data.vad_threshold));
    }
    clamp_int(kHotkeyCodeKey, data.hotkey_code, 1, 254);
    clamp_int(kCleanupMinWordsKey, data.cleanup_min_words, 1, 50);
    clamp_int(kCleanupTimeoutMsKey, data.cleanup_timeout_ms, 100, 60000);
    clamp_int(kMinHoldMsKey, data.min_hold_ms, 0, 2000);
    clamp_int(kHandsFreeSilenceMsKey, data.hands_free_silence_ms, 0, 10000);
    clamp_int(kMaxRecordingMsKey, data.max_recording_ms, 1000, 600000);
    clamp_int(kLlmContextSizeKey, data.llm_context_size, 512, 131072);
    clamp_int(kLlmMaxPredictTokensKey, data.llm_max_predict_tokens, 16, 8192);
    clamp_int(kLlmGpuLayersKey, data.llm_gpu_layers, -1, 999);
    clamp_int(kInjectRestoreDelayMsKey, data.inject_restore_delay_ms, 50, 5000);
    clamp_int(kWhisperThreadsKey, data.whisper_threads, 1, 16);
    clamp_int(kWhisperBeamSizeKey, data.whisper_beam_size, 1, 12);

    for (const std::string& warning : warnings_) {
        std::cerr << "[cfg] " << warning << '\n';
    }
    return data;
}
