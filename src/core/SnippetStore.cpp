#include "core/SnippetStore.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <utility>

namespace {

std::string ToLowerUtf8Safe(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) {
            out.push_back(static_cast<char>(std::tolower(u)));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string LowerTrimCopy(const std::string& text) {
    return ToLowerUtf8Safe(TrimCopy(text));
}

bool IsWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c >= 0x80;
}

std::size_t WholeWordFind(const std::string& lowered_text, const std::string& lowered_trigger,
                          std::size_t from) {
    std::size_t pos = from;
    for (;;) {
        pos = lowered_text.find(lowered_trigger, pos);
        if (pos == std::string::npos) {
            return std::string::npos;
        }
        const bool left_ok =
            pos == 0 || !IsWordChar(lowered_text[pos - 1]);
        const std::size_t after = pos + lowered_trigger.size();
        const bool right_ok =
            after >= lowered_text.size() || !IsWordChar(lowered_text[after]);
        if (left_ok && right_ok) {
            return pos;
        }
        ++pos;
    }
}

std::size_t CountCodePoints(const std::string& text) {
    std::size_t count = 0;
    for (const char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

std::string CollapseWhitespace(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool pending_space = false;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!out.empty()) {
                pending_space = true;
            }
            continue;
        }
        if (pending_space) {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(c);
    }
    return out;
}

}

std::string SnippetStore::Normalize(const std::string& text) {
    std::string trimmed = TrimCopy(text);
    while (!trimmed.empty() &&
           (std::ispunct(static_cast<unsigned char>(trimmed.back())) != 0 ||
            std::isspace(static_cast<unsigned char>(trimmed.back())) != 0)) {
        trimmed.pop_back();
    }
    std::size_t begin = 0;
    while (begin < trimmed.size() &&
           std::ispunct(static_cast<unsigned char>(trimmed[begin])) != 0) {
        ++begin;
    }
    return CollapseWhitespace(ToLowerUtf8Safe(trimmed.substr(begin)));
}

std::int64_t SnippetStore::NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string SnippetStore::GenerateIdLocked() const {
    const std::int64_t now = NowSeconds();
    std::string base = "snippet-" + std::to_string(now);
    for (std::size_t suffix = 1;; ++suffix) {
        const std::string candidate = base + "-" + std::to_string(suffix);
        const bool taken =
            std::any_of(entries_.begin(), entries_.end(), [&candidate](const Entry& entry) {
                return entry.id == candidate;
            });
        if (!taken) {
            return candidate;
        }
    }
}

bool SnippetStore::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception&) {
        return false;
    }

    nlohmann::json items;
    if (json.is_array()) {
        items = json;
    } else if (json.is_object()) {
        if (json.contains("snippets") && json["snippets"].is_array()) {
            items = json["snippets"];
        } else if (json.contains("entries") && json["entries"].is_array()) {
            items = json["entries"];
        } else {
            return false;
        }
    } else {
        return false;
    }

    std::vector<Entry> entries;
    std::vector<std::string> warnings;
    std::set<std::string> reserved_ids;
    for (const auto& element : items) {
        if (element.is_object() && element.contains("id") && element["id"].is_string()) {
            const std::string id = element["id"].get<std::string>();
            if (!id.empty()) {
                reserved_ids.insert(id);
            }
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        const nlohmann::json& element = items[index];
        if (!element.is_object() || !element.contains("trigger") ||
            !element.contains("expansion") || !element["trigger"].is_string() ||
            !element["expansion"].is_string()) {
            continue;
        }
        Entry entry;
        entry.trigger = element["trigger"].get<std::string>();
        entry.expansion = element["expansion"].get<std::string>();
        if (element.contains("id") && element["id"].is_string()) {
            entry.id = element["id"].get<std::string>();
        }
        if (element.contains("created_at") && element["created_at"].is_number()) {
            entry.created_at = element["created_at"].get<std::int64_t>();
        }
        if (element.contains("updated_at") && element["updated_at"].is_number()) {
            entry.updated_at = element["updated_at"].get<std::int64_t>();
        }
        if (TrimCopy(entry.trigger).empty()) {
            continue;
        }
        if (CountCodePoints(entry.expansion) > kMaxExpansionChars) {
            warnings.push_back("snippet '" + entry.trigger +
                               "' rejected: expansion over 4000 chars");
            continue;
        }
        const std::string key = LowerTrimCopy(entry.trigger);
        const auto duplicate = std::find_if(
            entries.begin(), entries.end(),
            [&key](const Entry& existing) { return LowerTrimCopy(existing.trigger) == key; });
        if (duplicate != entries.end()) {
            warnings.push_back("duplicate snippet trigger '" + entry.trigger +
                               "', last wins");
            entries.erase(duplicate);
        }
        const bool id_taken =
            std::any_of(entries.begin(), entries.end(),
                        [&entry](const Entry& existing) { return existing.id == entry.id; });
        if (entry.id.empty() || id_taken) {
            std::size_t suffix = index + 1;
            std::string candidate = "snippet-" + std::to_string(suffix);
            while (reserved_ids.count(candidate) != 0) {
                ++suffix;
                candidate = "snippet-" + std::to_string(suffix);
            }
            entry.id = candidate;
            reserved_ids.insert(candidate);
        }
        entries.push_back(std::move(entry));
    }
    if (!warnings.empty()) {
        lwf::Log::Warning("snippets: " + std::to_string(warnings.size()) + " load warnings");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    entries_ = std::move(entries);
    warnings_ = std::move(warnings);
    path_ = path;
    loaded_ = true;
    return true;
}

bool SnippetStore::Save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json json;
    json["version"] = 2;
    nlohmann::json items = nlohmann::json::array();
    for (const Entry& entry : entries_) {
        nlohmann::json element;
        element["id"] = entry.id;
        element["trigger"] = entry.trigger;
        element["expansion"] = entry.expansion;
        element["created_at"] = entry.created_at;
        element["updated_at"] = entry.updated_at;
        items.push_back(std::move(element));
    }
    json["snippets"] = std::move(items);

    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << json.dump(4) << '\n';
        tmp.flush();
        if (!tmp) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::copy_file(path, path + ".bak",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ec.clear();
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}

bool SnippetStore::Save() const {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        path = path_;
    }
    if (path.empty()) {
        return false;
    }
    return Save(path);
}

void SnippetStore::SetEntries(std::vector<Entry> entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::int64_t now = NowSeconds();
    for (Entry& entry : entries) {
        if (entry.id.empty()) {
            std::string base = "snippet-" + std::to_string(now);
            for (std::size_t suffix = 1;; ++suffix) {
                const std::string candidate = base + "-" + std::to_string(suffix);
                const bool taken = std::any_of(entries.begin(), entries.end(),
                                               [&candidate](const Entry& other) {
                                                   return other.id == candidate;
                                               });
                if (!taken) {
                    entry.id = candidate;
                    break;
                }
            }
        }
        if (entry.created_at == 0 && entry.updated_at == 0) {
            entry.created_at = now;
            entry.updated_at = now;
        }
    }
    entries_ = std::move(entries);
    warnings_.clear();
    loaded_ = true;
}

void SnippetStore::SetMode(std::string mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = std::move(mode);
}

std::vector<SnippetStore::Entry> SnippetStore::Entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

std::vector<std::string> SnippetStore::Warnings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return warnings_;
}

bool SnippetStore::Loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

std::string SnippetStore::Expand(const std::string& text) const {
    if (text.empty()) {
        return text;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.empty()) {
        return text;
    }

    if (mode_ != "contains") {
        const std::string normalized = Normalize(text);
        const Entry* best = nullptr;
        for (const Entry& entry : entries_) {
            if (TrimCopy(entry.trigger).empty()) {
                continue;
            }
            if (Normalize(entry.trigger) == normalized &&
                (best == nullptr || entry.trigger.size() > best->trigger.size())) {
                best = &entry;
            }
        }
        return best != nullptr ? best->expansion : text;
    }

    std::vector<const Entry*> ordered;
    ordered.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        ordered.push_back(&entry);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const Entry* lhs, const Entry* rhs) {
                  if (lhs->trigger.size() != rhs->trigger.size()) {
                      return lhs->trigger.size() > rhs->trigger.size();
                  }
                  return lhs->trigger < rhs->trigger;
              });

    std::string out = text;
    for (const Entry* entry : ordered) {
        const std::string trigger = ToLowerUtf8Safe(TrimCopy(entry->trigger));
        if (trigger.empty()) {
            continue;
        }
        std::size_t search_from = 0;
        for (;;) {
            const std::string lowered_out = ToLowerUtf8Safe(out);
            const std::size_t pos = WholeWordFind(lowered_out, trigger, search_from);
            if (pos == std::string::npos) {
                break;
            }
            out = out.substr(0, pos) + entry->expansion + out.substr(pos + trigger.size());
            search_from = pos + entry->expansion.size();
        }
    }
    return out;
}

std::string SnippetStore::ValidateLocked(const std::string& trigger,
                                         const std::string& expansion,
                                         const std::string& exclude_id) const {
    if (TrimCopy(trigger).empty()) {
        return "Please enter a word";
    }
    if (CountCodePoints(expansion) > kMaxExpansionChars) {
        return "Snippet too long";
    }
    if (TrimCopy(expansion).empty()) {
        return "Please enter a snippet";
    }
    const std::string trigger_key = LowerTrimCopy(trigger);
    const std::string expansion_key = LowerTrimCopy(expansion);
    for (const Entry& entry : entries_) {
        const bool is_self = (!exclude_id.empty() && entry.id == exclude_id) ||
                             (exclude_id.empty() &&
                              LowerTrimCopy(entry.trigger) == trigger_key);
        if (is_self) {
            continue;
        }
        const std::string other_trigger = LowerTrimCopy(entry.trigger);
        const std::string other_expansion = LowerTrimCopy(entry.expansion);
        if (other_trigger == trigger_key || other_trigger == expansion_key ||
            other_expansion == trigger_key || other_expansion == expansion_key) {
            return "Snippet already exists";
        }
    }
    return {};
}

std::string SnippetStore::Validate(const std::string& trigger,
                                   const std::string& expansion) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ValidateLocked(trigger, expansion, "");
}

std::string SnippetStore::Validate(const std::string& trigger, const std::string& expansion,
                                   const std::string& exclude_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ValidateLocked(trigger, expansion, exclude_id);
}

bool SnippetStore::Upsert(const std::string& trigger, const std::string& expansion,
                          std::string& error) {
    return Upsert(trigger, expansion, "", error);
}

bool SnippetStore::Upsert(const std::string& trigger, const std::string& expansion,
                          const std::string& exclude_id, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (TrimCopy(trigger).empty()) {
        error = "Please enter a word";
        return false;
    }
    if (CountCodePoints(expansion) > kMaxExpansionChars) {
        error = "Snippet too long";
        return false;
    }
    const std::string trigger_key = LowerTrimCopy(trigger);
    if (TrimCopy(expansion).empty()) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                       [&trigger_key, &exclude_id](const Entry& entry) {
                                           if (!exclude_id.empty()) {
                                               return entry.id == exclude_id;
                                           }
                                           return LowerTrimCopy(entry.trigger) == trigger_key;
                                       }),
                       entries_.end());
        error.clear();
        return true;
    }
    const std::string validation = ValidateLocked(trigger, expansion, exclude_id);
    if (!validation.empty()) {
        error = validation;
        return false;
    }
    const std::int64_t now = NowSeconds();
    const auto existing = std::find_if(
        entries_.begin(), entries_.end(), [&trigger_key, &exclude_id](const Entry& entry) {
            if (!exclude_id.empty()) {
                return entry.id == exclude_id;
            }
            return LowerTrimCopy(entry.trigger) == trigger_key;
        });
    if (existing != entries_.end()) {
        if (existing->id.empty()) {
            existing->id = GenerateIdLocked();
        }
        if (existing->created_at == 0) {
            existing->created_at = now;
        }
        existing->trigger = TrimCopy(trigger);
        existing->expansion = expansion;
        existing->updated_at = now;
    } else {
        Entry entry;
        entry.trigger = TrimCopy(trigger);
        entry.expansion = expansion;
        entry.id = GenerateIdLocked();
        entry.created_at = now;
        entry.updated_at = now;
        entries_.push_back(std::move(entry));
    }
    error.clear();
    loaded_ = true;
    return true;
}

void SnippetStore::RemoveByTrigger(const std::string& trigger) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string key = LowerTrimCopy(trigger);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                   [&key](const Entry& entry) {
                                       return LowerTrimCopy(entry.trigger) == key;
                                   }),
                   entries_.end());
}

void SnippetStore::RemoveById(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                   [&id](const Entry& entry) { return entry.id == id; }),
                   entries_.end());
}
