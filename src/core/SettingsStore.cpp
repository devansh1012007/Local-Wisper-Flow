#include "core/SettingsStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

const std::vector<SettingsStore::SchemaEntry>& BuildSchema() {
    static const std::vector<SettingsStore::SchemaEntry> schema = {
        {"schema_version", SettingsStore::Type::Int, 1, "", 1, 1, false},
        {"hotkey_code", SettingsStore::Type::Int, 165, "", 0, 255, true},
        {"hotkey_ptt", SettingsStore::Type::String, 0, "", 0, 0, true},
        {"hotkey_handsfree", SettingsStore::Type::String, 0, "", 0, 0, true},
        {"hotkey_cancel", SettingsStore::Type::String, 0, "Escape", 0, 0, true},
        {"hotkey_command", SettingsStore::Type::String, 0, "", 0, 0, true},
        {"hotkey_paste_last", SettingsStore::Type::String, 0, "", 0, 0, true},
        {"double_tap_handsfree", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"double_tap_window_ms", SettingsStore::Type::Int, 400, "", 100, 2000, true},
        {"min_hold_ms", SettingsStore::Type::Int, 250, "", 50, 5000, true},
        {"max_recording_ms", SettingsStore::Type::Int, 30000, "", 1000, 120000, true},
        {"hands_free_silence_ms", SettingsStore::Type::Int, 1500, "", 0, 10000, true},
        {"avoid_altgr_conflict", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"whisper_model_path", SettingsStore::Type::String, 0, "ggml-base.en.bin", 0, 0, false},
        {"whisper_language", SettingsStore::Type::String, 0, "auto", 0, 0, true},
        {"vad_threshold", SettingsStore::Type::Double, 0.6, "", 0.0, 1.0, true},
        {"llm_model_path", SettingsStore::Type::String, 0, "", 0, 0, false},
        {"cleanup_enabled", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"cleanup_min_words", SettingsStore::Type::Int, 4, "", 1, 100, true},
        {"cleanup_timeout_ms", SettingsStore::Type::Int, 2000, "", 200, 30000, true},
        {"cleanup_language_policy", SettingsStore::Type::String, 0, "keep", 0, 0, true},
        {"spoken_commands_aggressive", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"dictionary_fuzzy", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"snippet_mode", SettingsStore::Type::String, 0, "whole", 0, 0, true},
        {"command_timeout_ms", SettingsStore::Type::Int, 8000, "", 500, 60000, true},
        {"use_remote", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"remote_endpoint", SettingsStore::Type::String, 0, "localhost:50051", 0, 0, true},
        {"inject_delay_ms", SettingsStore::Type::Int, 0, "", 0, 500, true},
        {"streaming_stt", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"streaming_min_segment_ms", SettingsStore::Type::Int, 3000, "", 1000, 30000, true},
        {"streaming_pause_ms", SettingsStore::Type::Int, 700, "", 300, 5000, true},
        {"stt_provider", SettingsStore::Type::String, 0, "local", 0, 0, true},
        {"stt_consent_accepted", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"stt_language", SettingsStore::Type::String, 0, "indic-auto", 0, 0, true},
        {"stt_timeout_ms", SettingsStore::Type::Int, 15000, "", 1000, 60000, true},
        {"stt_max_clips", SettingsStore::Type::Int, 40, "", 1, 200, true},
        {"mic_device", SettingsStore::Type::String, 0, "", 0, 0, false},
        {"sound_enabled", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"sound_volume", SettingsStore::Type::Double, 0.3, "", 0.0, 1.0, true},
        {"reduce_motion", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"mini_x", SettingsStore::Type::Int, -1, "", -1000000, 1000000, false},
        {"mini_y", SettingsStore::Type::Int, -1, "", -1000000, 1000000, false},
        {"history_enabled", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"keep_raw_in_history", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"history_max_entries", SettingsStore::Type::Int, 1000, "", 10, 100000, true},
        {"history_max_age_days", SettingsStore::Type::Int, 90, "", 1, 3650, true},
        {"debug_log_transcripts", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"dictation_enabled", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"first_run", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"scratchpad_flow_bar", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"keep_usage_stats", SettingsStore::Type::Bool, 1, "", 0, 0, true},
        {"last_page", SettingsStore::Type::String, 0, "Home", 0, 0, true},
        {"quiet_speech_boost", SettingsStore::Type::Bool, 0, "", 0, 0, true},
        {"llm_provider", SettingsStore::Type::String, 0, "local", 0, 0, false},
        {"openrouter_model", SettingsStore::Type::String, 0, "", 0, 0, false},
    };
    return schema;
}

}

const std::vector<SettingsStore::SchemaEntry>& SettingsStore::Schema() {
    return BuildSchema();
}

SettingsStore::SettingsStore(std::string path) : path_(std::move(path)) {
    Load();
}

const SettingsStore::SchemaEntry* SettingsStore::FindSchema(const std::string& key) const {
    for (const SchemaEntry& entry : BuildSchema()) {
        if (key == entry.key) {
            return &entry;
        }
    }
    return nullptr;
}

void SettingsStore::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    clamped_.clear();
    corrupt_ = false;
    LoadLocked();
}

void SettingsStore::LoadLocked() {

    // INVARIANT: a directory at path_ is treated as a missing config
    // (defaults, loaded_existing=false, corrupt_=false) and is never renamed.
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path_, rc_ec) || rc_ec) {
        loaded_existing_ = false;
        raw_unknown_.clear();
        for (const SchemaEntry& entry : BuildSchema()) {
            if (entry.type != Type::String) {
                numbers_[entry.key] = entry.default_value;
            } else {
                strings_[entry.key] = entry.default_string;
            }
            if (entry.type == Type::Bool) {
                bools_[entry.key] = entry.default_value != 0.0;
            }
        }
        return;
    }

    std::ifstream file(path_);
    if (!file.is_open()) {
        loaded_existing_ = false;
        raw_unknown_.clear();
        for (const SchemaEntry& entry : BuildSchema()) {
            if (entry.type != Type::String) {
                numbers_[entry.key] = entry.default_value;
            } else {
                strings_[entry.key] = entry.default_string;
            }
            if (entry.type == Type::Bool) {
                bools_[entry.key] = entry.default_value != 0.0;
            }
        }
        return;
    }

    nlohmann::json json;
    try {
        file >> json;
        if (!json.is_object()) {
            throw std::runtime_error("config root must be an object");
        }
        loaded_existing_ = true;
    } catch (const std::exception&) {
        corrupt_ = true;
        loaded_existing_ = false;
        file.close();
        std::error_code ec;
        std::filesystem::rename(path_, path_ + ".bad", ec);
        LoadLocked();
        corrupt_ = true;
        return;
    }

    for (auto it = json.begin(); it != json.end(); ++it) {
        const SchemaEntry* entry = FindSchema(it.key());
        if (entry == nullptr) {
            continue;
        }
        switch (entry->type) {
            case Type::Int:
            case Type::Double: {
                double value = 0.0;
                if (it->is_number()) {
                    value = it->get<double>();
                } else {
                    value = entry->default_value;
                }
                if (value < entry->min_value || value > entry->max_value) {
                    value = std::clamp(value, entry->min_value, entry->max_value);
                    clamped_.push_back(it.key());
                }
                numbers_[it.key()] = value;
                if (entry->type == Type::Bool) {
                    bools_[it.key()] = value != 0.0;
                }
                break;
            }
            case Type::Bool:
                bools_[it.key()] = it->is_boolean() ? it->get<bool>() : entry->default_value != 0.0;
                numbers_[it.key()] = bools_[it.key()] ? 1.0 : 0.0;
                break;
            case Type::String:
                strings_[it.key()] = it->is_string() ? it->get<std::string>()
                                                     : std::string(entry->default_string);
                break;
        }
    }

    for (const SchemaEntry& entry : BuildSchema()) {
        if (json.find(entry.key) == json.end()) {
            if (entry.type != Type::String) {
                numbers_[entry.key] = entry.default_value;
            } else {
                strings_[entry.key] = entry.default_string;
            }
            if (entry.type == Type::Bool) {
                bools_[entry.key] = entry.default_value != 0.0;
            }
        }
    }

    raw_unknown_.clear();
    nlohmann::json unknown;
    for (auto it = json.begin(); it != json.end(); ++it) {
        if (FindSchema(it.key()) == nullptr) {
            unknown[it.key()] = *it;
        }
    }
    raw_unknown_ = unknown.dump();
}

void SettingsStore::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json json;
    if (!raw_unknown_.empty()) {
        try {
            json = nlohmann::json::parse(raw_unknown_, nullptr, false);
        } catch (const std::exception&) {
            json = nlohmann::json::object();
        }
        if (!json.is_object()) {
            json = nlohmann::json::object();
        }
    }
    for (const auto& [key, value] : numbers_) {
        const SchemaEntry* entry = FindSchema(key);
        if (entry != nullptr && entry->type == Type::Int) {
            json[key] = static_cast<int>(std::llround(value));
        } else if (entry != nullptr && entry->type == Type::Bool) {
            json[key] = value != 0.0;
        } else {
            json[key] = value;
        }
    }
    for (const auto& [key, value] : strings_) {
        json[key] = value;
    }

    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return;
        }
        tmp << json.dump(4) << '\n';
        tmp.flush();
    }
    std::error_code ec;
    std::filesystem::copy_file(path_, path_ + ".bak", std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::rename(tmp_path, path_, ec);
}

double SettingsStore::GetNumber(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = numbers_.find(key);
    return it != numbers_.end() ? it->second : 0.0;
}

int SettingsStore::GetInt(const std::string& key) const {
    return static_cast<int>(std::llround(GetDouble(key)));
}

double SettingsStore::GetDouble(const std::string& key) const {
    return GetNumber(key);
}

bool SettingsStore::GetBool(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bools_.find(key);
    return it != bools_.end() ? it->second : false;
}

std::string SettingsStore::GetString(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = strings_.find(key);
    return it != strings_.end() ? it->second : std::string();
}

void SettingsStore::SetNumber(const std::string& key, double value) {
    const SchemaEntry* entry = FindSchema(key);
    if (entry == nullptr) {
        return;
    }
    if (entry->type != Type::Bool &&
        (value < entry->min_value || value > entry->max_value)) {
        value = std::clamp(value, entry->min_value, entry->max_value);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        numbers_[key] = value;
        if (entry->type == Type::Bool) {
            bools_[key] = value != 0.0;
        }
    }
    Notify(key);
}

void SettingsStore::SetInt(const std::string& key, int value) {
    SetNumber(key, static_cast<double>(value));
}

void SettingsStore::SetDouble(const std::string& key, double value) {
    SetNumber(key, value);
}

void SettingsStore::SetBool(const std::string& key, bool value) {
    SetNumber(key, value ? 1.0 : 0.0);
}

void SettingsStore::SetString(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        strings_[key] = value;
    }
    Notify(key);
}

bool SettingsStore::HasKey(const std::string& key) const {
    const SchemaEntry* entry = FindSchema(key);
    if (entry == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return entry->type == Type::String ? strings_.count(key) > 0 : numbers_.count(key) > 0;
}

bool SettingsStore::IsHotApply(const std::string& key) const {
    const SchemaEntry* entry = FindSchema(key);
    return entry != nullptr && entry->hot_apply;
}

std::vector<std::string> SettingsStore::ClampedOnLastLoad() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clamped_;
}

bool SettingsStore::CorruptOnLastLoad() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return corrupt_;
}

bool SettingsStore::LoadedExistingFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_existing_;
}

void SettingsStore::AddListener(const std::string& key, Listener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[key].push_back(std::move(listener));
}

void SettingsStore::Notify(const std::string& key) {
    std::vector<Listener> to_call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = listeners_.find(key);
        if (it != listeners_.end()) {
            to_call = it->second;
        }
    }
    for (Listener& listener : to_call) {
        listener(key);
    }
}
