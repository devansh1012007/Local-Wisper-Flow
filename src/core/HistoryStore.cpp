#include "core/HistoryStore.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

HistoryStore::HistoryStore(std::string path, bool enabled, bool keep_raw, int max_entries,
                           int max_age_days)
    : path_(std::move(path)),
      enabled_(enabled),
      keep_raw_(keep_raw),
      max_entries_(max_entries),
      max_age_days_(max_age_days) {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadAllLocked();
    CompactLocked(NowSeconds());
}

void HistoryStore::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

bool HistoryStore::Enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

std::int64_t HistoryStore::NowSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void HistoryStore::LoadAllLocked() {
    entries_.clear();
    std::ifstream file(path_);
    if (!file.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const nlohmann::json json = nlohmann::json::parse(line);
            HistoryEntry entry;
            entry.timestamp = json.value("ts", std::int64_t{0});
            entry.raw = json.value("raw", std::string());
            entry.text = json.value("text", std::string());
            entry.app_profile = json.value("app", std::string());
            entry.duration_ms = json.value("dur", std::int64_t{0});
            entry.language = json.value("lang", std::string());
            if (!entry.text.empty()) {
                entries_.push_back(std::move(entry));
            }
        } catch (const nlohmann::json::exception&) {
            continue;
        }
    }
}

void HistoryStore::CompactLocked(std::int64_t now_seconds) {
    const std::int64_t cutoff = now_seconds - static_cast<std::int64_t>(max_age_days_) * 86400;
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [cutoff](const HistoryEntry& entry) {
                                      return entry.timestamp < cutoff;
                                  }),
                   entries_.end());
    if (static_cast<int>(entries_.size()) > max_entries_) {
        const std::size_t keep = static_cast<std::size_t>(max_entries_);
        entries_.erase(entries_.begin(), entries_.end() - keep);
    }

    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return;
        }
        for (const HistoryEntry& entry : entries_) {
            nlohmann::json json;
            json["ts"] = entry.timestamp;
            json["text"] = entry.text;
            if (keep_raw_ && !entry.raw.empty()) {
                json["raw"] = entry.raw;
            }
            if (!entry.app_profile.empty()) {
                json["app"] = entry.app_profile;
            }
            if (entry.duration_ms > 0) {
                json["dur"] = entry.duration_ms;
            }
            if (!entry.language.empty()) {
                json["lang"] = entry.language;
            }
            tmp << json.dump() << '\n';
        }
        tmp.flush();
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path_, ec);
}

void HistoryStore::Append(const HistoryEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) {
        return;
    }
    entries_.push_back(entry);
    ++appends_since_compaction_;
    if (appends_since_compaction_ >= 100) {
        appends_since_compaction_ = 0;
        CompactLocked(NowSeconds());
        return;
    }
    std::ofstream file(path_, std::ios::app);
    if (!file.is_open()) {
        return;
    }
    nlohmann::json json;
    json["ts"] = entry.timestamp;
    json["text"] = entry.text;
    if (keep_raw_ && !entry.raw.empty()) {
        json["raw"] = entry.raw;
    }
    if (!entry.app_profile.empty()) {
        json["app"] = entry.app_profile;
    }
    if (entry.duration_ms > 0) {
        json["dur"] = entry.duration_ms;
    }
    if (!entry.language.empty()) {
        json["lang"] = entry.language;
    }
    file << json.dump() << '\n';
}

std::vector<HistoryEntry> HistoryStore::Search(const std::string& substring,
                                               std::size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HistoryEntry> results;
    for (auto it = entries_.rbegin(); it != entries_.rend() && results.size() < limit; ++it) {
        if (substring.empty() || it->text.find(substring) != std::string::npos) {
            results.push_back(*it);
        }
    }
    return results;
}

std::optional<HistoryEntry> HistoryStore::Last() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_.back();
}

void HistoryStore::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    appends_since_compaction_ = 0;
    std::ofstream file(path_, std::ios::trunc);
}

bool HistoryStore::Export(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    for (const HistoryEntry& entry : entries_) {
        file << entry.timestamp << '\t' << entry.app_profile << '\t' << entry.text << '\n';
    }
    return true;
}

std::size_t HistoryStore::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}
