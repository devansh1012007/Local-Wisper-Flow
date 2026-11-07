#include "core/StatsStore.h"
#include "core/AppContextResolver.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <utility>

namespace {

std::int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string ToLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string NormalizeProcess(const std::string& process_name) {
    std::string normalized = ToLower(process_name);
    if (normalized.empty()) {
        return "unknown";
    }
    return normalized;
}

bool IsWhitespace(std::uint32_t cp) {
    if (cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C ||
        cp == 0x0D || cp == 0x20 || cp == 0x85 || cp == 0xA0 ||
        cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 ||
        cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000 ||
        cp == 0xFEFF) {
        return true;
    }
    return false;
}

void EnsureCategories(DayStats& day) {
    for (const std::string& name : AppContextResolver::CategoryNames()) {
        if (day.by_category.find(name) == day.by_category.end()) {
            day.by_category[name] = 0;
        }
    }
}

std::int64_t JsonNumber(const nlohmann::json& obj, const char* key) {
    if (obj.contains(key) && obj[key].is_number()) {
        return obj[key].get<std::int64_t>();
    }
    return 0;
}

}

std::string StatsStore::DateKey(std::int64_t timestamp_local) {
    std::int64_t ts = timestamp_local;
    if (ts <= 0) {
        ts = NowSeconds();
    }
    const std::time_t t = static_cast<std::time_t>(ts);
    std::tm tmval{};
#ifdef _WIN32
    if (localtime_s(&tmval, &t) != 0) {
        return "1970-01-01";
    }
#else
    if (localtime_r(&t, &tmval) == nullptr) {
        return "1970-01-01";
    }
#endif
    char buffer[16]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tmval) == 0) {
        return "1970-01-01";
    }
    return std::string(buffer);
}

std::int64_t StatsStore::CountWords(const std::string& text) {
    std::int64_t words = 0;
    bool in_word = false;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = c;
        std::size_t len = 1;
        if (c >= 0x80) {
            if ((c >> 5) == 0x6 && i + 1 < text.size()) {
                cp = (c & 0x1F);
                len = 2;
            } else if ((c >> 4) == 0xE && i + 2 < text.size()) {
                cp = (c & 0x0F);
                len = 3;
            } else if ((c >> 3) == 0x1E && i + 3 < text.size()) {
                cp = (c & 0x07);
                len = 4;
            } else {
                cp = 0xFFFD;
                len = 1;
            }
            bool valid = true;
            for (std::size_t k = 1; k < len; ++k) {
                const unsigned char cc = static_cast<unsigned char>(text[i + k]);
                if ((cc & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
                cp = static_cast<std::uint32_t>((cp << 6) | (cc & 0x3F));
            }
            if (!valid) {
                cp = 0xFFFD;
                len = 1;
            }
        }
        if (IsWhitespace(cp)) {
            in_word = false;
        } else if (!in_word) {
            ++words;
            in_word = true;
        }
        i += len;
    }
    return words;
}

StatsStore::StatsStore(std::string path, bool enabled)
    : path_(std::move(path)), enabled_(enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    LoadLocked();
    if (EvictLocked()) {
        PersistLocked();
    }
}

void StatsStore::Record(const DictationRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) {
        return;
    }
    const std::int64_t words = CountWords(record.final_text);
    if (words == 0) {
        return;
    }
    std::int64_t ts = record.timestamp_local;
    if (ts <= 0) {
        ts = NowSeconds();
    }
    const std::string key = DateKey(ts);
    const std::int64_t duration = record.duration_ms < 0 ? 0 : record.duration_ms;
    const std::int64_t corrected = record.words_corrected < 0 ? 0 : record.words_corrected;
    const std::int64_t fixes = record.dictionary_fixes < 0 ? 0 : record.dictionary_fixes;
    const std::string category = AppContextResolver::CategoryFor(record.process_name, "");
    const std::string process = NormalizeProcess(record.process_name);
    DayStats& day = days_[key];
    EnsureCategories(day);
    day.words += words;
    day.dictations += 1;
    day.speaking_ms += duration;
    day.words_corrected += corrected;
    day.dictionary_fixes += fixes;
    day.by_category[category] += words;
    day.apps[process] += words;
    EvictLocked();
    PersistLocked();
    generation_.fetch_add(1);
}

void StatsStore::SetUsageStatsEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

bool StatsStore::UsageStatsEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

std::map<std::string, DayStats> StatsStore::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return days_;
}

std::uint64_t StatsStore::Generation() const noexcept {
    return generation_.load();
}

void StatsStore::LoadLocked() {
    days_.clear();
    std::ifstream file(path_);
    if (!file.is_open()) {
        return;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception&) {
        days_.clear();
        return;
    } catch (const std::exception&) {
        days_.clear();
        return;
    }
    if (!json.is_object()) {
        days_.clear();
        return;
    }
    for (auto it = json.begin(); it != json.end(); ++it) {
        if (!it->is_object()) {
            continue;
        }
        DayStats day;
        day.words = JsonNumber(*it, "words");
        day.dictations = JsonNumber(*it, "dictations");
        day.speaking_ms = JsonNumber(*it, "speaking_ms");
        day.words_corrected = JsonNumber(*it, "words_corrected");
        day.dictionary_fixes = JsonNumber(*it, "dictionary_fixes");
        if (it->contains("by_category") &&
            (*it)["by_category"].is_object()) {
            for (auto cat_it = (*it)["by_category"].begin();
                 cat_it != (*it)["by_category"].end(); ++cat_it) {
                if (cat_it->is_number()) {
                    day.by_category[cat_it.key()] = cat_it->get<std::int64_t>();
                }
            }
        }
        if (it->contains("apps") && (*it)["apps"].is_object()) {
            for (auto app_it = (*it)["apps"].begin(); app_it != (*it)["apps"].end();
                 ++app_it) {
                if (app_it->is_number()) {
                    day.apps[app_it.key()] = app_it->get<std::int64_t>();
                }
            }
        }
        EnsureCategories(day);
        days_[it.key()] = std::move(day);
    }
}

void StatsStore::PersistLocked() {
    if (path_.empty()) {
        return;
    }
    nlohmann::json json = nlohmann::json::object();
    for (auto& entry : days_) {
        EnsureCategories(entry.second);
        nlohmann::json day_json;
        day_json["words"] = entry.second.words;
        day_json["dictations"] = entry.second.dictations;
        day_json["speaking_ms"] = entry.second.speaking_ms;
        day_json["words_corrected"] = entry.second.words_corrected;
        day_json["dictionary_fixes"] = entry.second.dictionary_fixes;
        nlohmann::json categories = nlohmann::json::object();
        for (const auto& kv : entry.second.by_category) {
            categories[kv.first] = kv.second;
        }
        day_json["by_category"] = categories;
        nlohmann::json apps = nlohmann::json::object();
        for (const auto& kv : entry.second.apps) {
            apps[kv.first] = kv.second;
        }
        day_json["apps"] = apps;
        json[entry.first] = day_json;
    }
    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return;
        }
        tmp << json.dump(4) << '\n';
        tmp.flush();
        if (!tmp) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path_, ec);
}

bool StatsStore::EvictLocked() {
    const std::int64_t now = NowSeconds();
    const std::string cutoff = DateKey(now - 729 * 86400);
    bool changed = false;
    for (auto it = days_.begin(); it != days_.end();) {
        if (it->first < cutoff) {
            it = days_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}
