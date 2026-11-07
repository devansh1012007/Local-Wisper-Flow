#include "core/ApiKeyStore.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <utility>

namespace {

void QuarantineFile(const std::string& path) {
    std::error_code ec;
    std::filesystem::rename(path, path + ".bad", ec);
    if (ec) {
        std::error_code remove_ec;
        std::filesystem::remove(path + ".bad", remove_ec);
        ec.clear();
        std::filesystem::rename(path, path + ".bad", ec);
    }
}

}  // namespace

ApiKeyStore::ApiKeyStore(std::string path) : path_(std::move(path)) {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadLocked();
}

void ApiKeyStore::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadLocked();
}

std::string ApiKeyStore::Get(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = keys_.find(provider);
    if (it == keys_.end()) {
        return {};
    }
    return it->second;
}

bool ApiKeyStore::Has(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = keys_.find(provider);
    return it != keys_.end() && !it->second.empty();
}

bool ApiKeyStore::Set(const std::string& provider, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (key.empty()) {
        const auto it = keys_.find(provider);
        if (it == keys_.end()) {
            return true;
        }
        const std::string backup = it->second;
        keys_.erase(it);
        if (!PersistLocked()) {
            keys_[provider] = backup;
            return false;
        }
        return true;
    }
    const auto it = keys_.find(provider);
    const bool had = it != keys_.end();
    const std::string backup = had ? it->second : std::string();
    keys_[provider] = key;
    if (!PersistLocked()) {
        if (had) {
            keys_[provider] = backup;
        } else {
            keys_.erase(provider);
        }
        return false;
    }
    return true;
}

bool ApiKeyStore::Remove(const std::string& provider) {
    return Set(provider, std::string());
}

void ApiKeyStore::LoadLocked() {
    keys_.clear();
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path_, rc_ec) || rc_ec) {
        return;
    }
    std::ifstream file(path_);
    if (!file.is_open()) {
        return;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception&) {
        file.close();
        QuarantineFile(path_);
        keys_.clear();
        return;
    }
    if (!json.is_object()) {
        file.close();
        QuarantineFile(path_);
        keys_.clear();
        return;
    }
    file.close();
    for (auto it = json.begin(); it != json.end(); ++it) {
        if (!it->is_string()) {
            continue;
        }
        const std::string value = it->get<std::string>();
        if (value.empty()) {
            continue;
        }
        keys_[it.key()] = value;
    }
}

bool ApiKeyStore::PersistLocked() {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [key, value] : keys_) {
        json[key] = value;
    }
    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << json.dump(4) << '\n';
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    if (std::filesystem::exists(path_, ec)) {
        std::filesystem::copy_file(path_, path_ + ".bak",
                                   std::filesystem::copy_options::overwrite_existing, ec);
        ec.clear();
    } else {
        ec.clear();
    }
    std::filesystem::rename(tmp_path, path_, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}
