#include "core/DictionaryStore.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string ToLowerCopy(const std::string& text) {
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

bool ContainsWholeWord(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.empty()) {
        return false;
    }
    const std::string lowered = ToLowerCopy(haystack);
    const std::string target = ToLowerCopy(needle);
    std::size_t pos = 0;
    for (;;) {
        pos = lowered.find(target, pos);
        if (pos == std::string::npos) {
            return false;
        }
        const std::size_t after = pos + target.size();
        const bool left_ok =
            pos == 0 || !std::isalnum(static_cast<unsigned char>(lowered[pos - 1]));
        const bool right_ok =
            after >= lowered.size() || !std::isalnum(static_cast<unsigned char>(lowered[after]));
        if (left_ok && right_ok) {
            return true;
        }
        ++pos;
    }
}

void MoveToBad(const std::string& path) {
    std::error_code ec;
    std::filesystem::rename(path, path + ".bad", ec);
    if (ec) {
        std::error_code remove_ec;
        std::filesystem::remove(path + ".bad", remove_ec);
        ec.clear();
        std::filesystem::rename(path, path + ".bad", ec);
    }
    if (ec) {
        lwf::Log::Warning("dictionary: could not rename corrupt file to .bad: " + ec.message());
    }
}

void MergeParsed(std::vector<DictionaryStore::Entry>& out,
                 std::unordered_map<std::string, std::size_t>& index,
                 DictionaryStore::Entry candidate) {
    const std::string key = ToLowerCopy(TrimCopy(candidate.phrase));
    if (key.empty()) {
        return;
    }
    candidate.phrase = TrimCopy(candidate.phrase);
    candidate.replacement = TrimCopy(candidate.replacement);
    if (!candidate.has_replacement || candidate.replacement.empty()) {
        candidate.has_replacement = false;
        candidate.replacement.clear();
    }
    const auto found = index.find(key);
    if (found == index.end()) {
        index.emplace(key, out.size());
        out.push_back(std::move(candidate));
        return;
    }
    DictionaryStore::Entry& existing = out[found->second];
    if (!existing.has_replacement && candidate.has_replacement) {
        existing.replacement = candidate.replacement;
        existing.has_replacement = true;
    }
    existing.starred = existing.starred || candidate.starred;
    if (candidate.usage_count > existing.usage_count) {
        existing.usage_count = candidate.usage_count;
    }
}

bool IsLegacyShape(const nlohmann::json& json) {
    if (json.is_array()) {
        return true;
    }
    if (json.is_object() && json.contains("words")) {
        return true;
    }
    if (json.is_object() && json.contains("replacements")) {
        return true;
    }
    return false;
}

void ParseLegacy(const nlohmann::json& json, std::int64_t now, DictionaryStore::Source source,
                 std::vector<DictionaryStore::Entry>& out) {
    std::unordered_map<std::string, std::size_t> index;
    const auto make_entry = [&](const std::string& phrase, const std::string& replacement,
                                bool has_replacement) {
        DictionaryStore::Entry entry;
        entry.phrase = phrase;
        entry.replacement = replacement;
        entry.has_replacement = has_replacement;
        entry.starred = false;
        entry.source = source;
        entry.usage_count = 0;
        entry.created_at = now;
        entry.updated_at = now;
        return entry;
    };
    if (json.is_array()) {
        for (const auto& element : json) {
            if (!element.is_string()) {
                continue;
            }
            const std::string phrase = TrimCopy(element.get<std::string>());
            if (phrase.empty()) {
                continue;
            }
            MergeParsed(out, index, make_entry(phrase, {}, false));
        }
        return;
    }
    if (json.contains("words") && json["words"].is_array()) {
        for (const auto& element : json["words"]) {
            if (!element.is_string()) {
                continue;
            }
            const std::string phrase = TrimCopy(element.get<std::string>());
            if (phrase.empty()) {
                continue;
            }
            MergeParsed(out, index, make_entry(phrase, {}, false));
        }
    }
    if (json.contains("replacements") && json["replacements"].is_array()) {
        for (const auto& element : json["replacements"]) {
            if (!element.is_object() || !element.contains("from") || !element.contains("to")) {
                continue;
            }
            if (!element["from"].is_string() || !element["to"].is_string()) {
                continue;
            }
            const std::string phrase = TrimCopy(element["from"].get<std::string>());
            const std::string replacement = TrimCopy(element["to"].get<std::string>());
            if (phrase.empty() || replacement.empty()) {
                continue;
            }
            MergeParsed(out, index, make_entry(phrase, replacement, true));
        }
    }
}

void ParseV2(const nlohmann::json& json, std::int64_t now,
             std::vector<DictionaryStore::Entry>& out) {
    std::unordered_map<std::string, std::size_t> index;
    if (!json.contains("entries") || !json["entries"].is_array()) {
        return;
    }
    for (const auto& element : json["entries"]) {
        if (!element.is_object()) {
            continue;
        }
        if (!element.contains("phrase") || !element["phrase"].is_string()) {
            continue;
        }
        const std::string phrase = TrimCopy(element["phrase"].get<std::string>());
        if (phrase.empty()) {
            continue;
        }
        DictionaryStore::Entry entry;
        entry.phrase = phrase;
        entry.replacement.clear();
        entry.has_replacement = false;
        if (element.contains("replacement") && element["replacement"].is_string()) {
            const std::string replacement = TrimCopy(element["replacement"].get<std::string>());
            if (!replacement.empty()) {
                entry.replacement = replacement;
                entry.has_replacement = true;
            }
        }
        entry.starred = false;
        if (element.contains("starred") && element["starred"].is_boolean()) {
            entry.starred = element["starred"].get<bool>();
        }
        entry.source = DictionaryStore::Source::Manual;
        if (element.contains("source") && element["source"].is_string()) {
            if (element["source"].get<std::string>() == "imported") {
                entry.source = DictionaryStore::Source::Imported;
            }
        }
        entry.usage_count = 0;
        if (element.contains("usage_count") && element["usage_count"].is_number()) {
            entry.usage_count = element["usage_count"].get<std::int64_t>();
        }
        if (entry.usage_count < 0) {
            entry.usage_count = 0;
        }
        entry.created_at = now;
        if (element.contains("created_at") && element["created_at"].is_number()) {
            entry.created_at = element["created_at"].get<std::int64_t>();
        }
        entry.updated_at = entry.created_at;
        if (element.contains("updated_at") && element["updated_at"].is_number()) {
            entry.updated_at = element["updated_at"].get<std::int64_t>();
        }
        entry.id = 0;
        if (element.contains("id") && element["id"].is_number()) {
            entry.id = element["id"].get<std::int64_t>();
        }
        MergeParsed(out, index, entry);
    }
    std::set<std::int64_t> used;
    std::int64_t next = 1;
    for (DictionaryStore::Entry& entry : out) {
        if (entry.id <= 0 || used.count(entry.id) != 0) {
            while (used.count(next) != 0) {
                ++next;
            }
            entry.id = next;
        }
        used.insert(entry.id);
        if (entry.id >= next) {
            next = entry.id + 1;
        }
    }
}

bool ParseAny(const nlohmann::json& json, std::int64_t now, DictionaryStore::Source legacy_source,
              std::vector<DictionaryStore::Entry>& out, bool& migrated) {
    migrated = false;
    if (json.is_object() && json.contains("schema_version")) {
        if (json["schema_version"].is_number() &&
            json["schema_version"].get<std::int64_t>() == 2) {
            ParseV2(json, now, out);
            return true;
        }
        return false;
    }
    if (IsLegacyShape(json)) {
        ParseLegacy(json, now, legacy_source, out);
        std::int64_t next = 1;
        for (DictionaryStore::Entry& entry : out) {
            entry.id = next++;
        }
        migrated = true;
        return true;
    }
    return false;
}

std::vector<DictionaryStore::Entry> SortedCopy(const std::vector<DictionaryStore::Entry>& source) {
    std::vector<DictionaryStore::Entry> sorted = source;
    std::sort(sorted.begin(), sorted.end(),
              [](const DictionaryStore::Entry& left, const DictionaryStore::Entry& right) {
                  if (left.starred != right.starred) {
                      return left.starred > right.starred;
                  }
                  if (left.updated_at != right.updated_at) {
                      return left.updated_at > right.updated_at;
                  }
                  if (left.created_at != right.created_at) {
                      return left.created_at > right.created_at;
                  }
                  return left.id > right.id;
              });
    return sorted;
}

nlohmann::json EntriesToJson(const std::vector<DictionaryStore::Entry>& entries) {
    nlohmann::json root = nlohmann::json::object();
    root["schema_version"] = 2;
    nlohmann::json array = nlohmann::json::array();
    for (const DictionaryStore::Entry& entry : entries) {
        nlohmann::json item = nlohmann::json::object();
        item["id"] = entry.id;
        item["phrase"] = entry.phrase;
        if (entry.has_replacement && !entry.replacement.empty()) {
            item["replacement"] = entry.replacement;
        }
        item["starred"] = entry.starred;
        item["source"] = entry.source == DictionaryStore::Source::Imported ? "imported" : "manual";
        item["usage_count"] = entry.usage_count;
        item["created_at"] = entry.created_at;
        item["updated_at"] = entry.updated_at;
        array.push_back(std::move(item));
    }
    root["entries"] = std::move(array);
    return root;
}

}

DictionaryStore::DictionaryStore(std::string path) : path_(std::move(path)) {}

std::int64_t DictionaryStore::NowSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void DictionaryStore::SeedLocked() {
    entries_.clear();
    const std::int64_t now = NowSeconds();
    Entry seed;
    seed.id = 1;
    seed.phrase = "btw";
    seed.replacement = "by the way";
    seed.has_replacement = true;
    seed.starred = false;
    seed.source = Source::Manual;
    seed.usage_count = 0;
    seed.created_at = now;
    seed.updated_at = now;
    entries_.push_back(seed);
    next_id_ = 2;
}

bool DictionaryStore::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty()) {
        return false;
    }
    std::ifstream file(path_);
    if (!file.is_open()) {
        SeedLocked();
        loaded_ = true;
        migrated_ = false;
        return true;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception&) {
        file.close();
        MoveToBad(path_);
        lwf::Log::Warning("dictionary: corrupt file renamed to .bad");
        entries_.clear();
        next_id_ = 1;
        loaded_ = false;
        migrated_ = false;
        return false;
    }
    std::vector<Entry> parsed;
    bool migrated = false;
    if (!ParseAny(json, NowSeconds(), Source::Manual, parsed, migrated)) {
        file.close();
        MoveToBad(path_);
        lwf::Log::Warning("dictionary: unsupported file renamed to .bad");
        entries_.clear();
        next_id_ = 1;
        loaded_ = false;
        migrated_ = false;
        return false;
    }
    bool truncated = false;
    if (parsed.size() > kMaxEntries) {
        lwf::Log::Warning("dictionary: truncated to " + std::to_string(kMaxEntries) + " entries");
        parsed.resize(kMaxEntries);
        truncated = true;
    }
    std::int64_t next = 1;
    for (const Entry& entry : parsed) {
        if (entry.id >= next) {
            next = entry.id + 1;
        }
    }
    entries_ = std::move(parsed);
    next_id_ = next;
    loaded_ = true;
    migrated_ = migrated;
    if (truncated) {
        bak_pending_ = true;
    }
    return true;
}

bool DictionaryStore::Reload() {
    return Load();
}

void DictionaryStore::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    next_id_ = 1;
    migrated_ = false;
    bak_pending_ = false;
    loaded_ = true;
    if (path_.empty()) {
        return;
    }
    const nlohmann::json root = EntriesToJson(entries_);
    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            lwf::Log::Warning("dictionary: could not write cleared file");
            return;
        }
        tmp << root.dump(4) << '\n';
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            lwf::Log::Warning("dictionary: could not write cleared file");
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path_, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        lwf::Log::Warning("dictionary: could not write cleared file");
    }
}

bool DictionaryStore::Save() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path_.empty()) {
        return false;
    }
    if (entries_.size() > kMaxEntries) {
        return false;
    }
    const nlohmann::json root = EntriesToJson(entries_);
    const std::string tmp_path = path_ + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << root.dump(4) << '\n';
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    if (migrated_ || bak_pending_) {
        std::error_code ec;
        if (!std::filesystem::exists(path_ + ".bak", ec)) {
            std::filesystem::copy_file(path_, path_ + ".bak",
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                lwf::Log::Warning("dictionary: could not create .bak: " + ec.message());
            }
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path_, ec);
    if (ec) {
        return false;
    }
    loaded_ = true;
    migrated_ = false;
    bak_pending_ = false;
    return true;
}

std::vector<DictionaryStore::Entry> DictionaryStore::Entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return SortedCopy(entries_);
}

std::vector<DictionaryStore::Entry> DictionaryStore::Search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string needle = ToLowerCopy(TrimCopy(query));
    std::vector<Entry> filtered;
    if (needle.empty()) {
        filtered = entries_;
    } else {
        for (const Entry& entry : entries_) {
            const std::string phrase = ToLowerCopy(entry.phrase);
            const std::string replacement =
                entry.has_replacement ? ToLowerCopy(entry.replacement) : std::string();
            if (phrase.find(needle) != std::string::npos ||
                (!replacement.empty() &&
                 replacement.find(needle) != std::string::npos)) {
                filtered.push_back(entry);
            }
        }
    }
    return SortedCopy(filtered);
}

bool DictionaryStore::Add(const std::string& phrase, const std::string& replacement,
                          std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string trimmed_phrase = TrimCopy(phrase);
    const std::string trimmed_replacement = TrimCopy(replacement);
    if (trimmed_phrase.empty()) {
        if (error != nullptr) {
            *error = "phrase cannot be empty";
        }
        return false;
    }
    if (entries_.size() >= kMaxEntries) {
        if (error != nullptr) {
            *error = "maximum 2000 entries reached";
        }
        return false;
    }
    const std::string key = ToLowerCopy(trimmed_phrase);
    for (const Entry& entry : entries_) {
        if (ToLowerCopy(TrimCopy(entry.phrase)) == key) {
            if (error != nullptr) {
                *error = "Word already exists.";
            }
            return false;
        }
    }
    if (!trimmed_replacement.empty() &&
        ContainsWholeWord(trimmed_replacement, trimmed_phrase)) {
        if (error != nullptr) {
            *error = "replacement cannot contain the phrase";
        }
        return false;
    }
    const std::int64_t now = NowSeconds();
    Entry entry;
    entry.id = next_id_++;
    entry.phrase = trimmed_phrase;
    entry.replacement = trimmed_replacement;
    entry.has_replacement = !trimmed_replacement.empty();
    if (!entry.has_replacement) {
        entry.replacement.clear();
    }
    entry.starred = false;
    entry.source = Source::Manual;
    entry.usage_count = 0;
    entry.created_at = now;
    entry.updated_at = now;
    entries_.push_back(entry);
    loaded_ = true;
    return true;
}

bool DictionaryStore::Update(std::int64_t id, const std::string& phrase,
                             const std::string& replacement, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string trimmed_phrase = TrimCopy(phrase);
    const std::string trimmed_replacement = TrimCopy(replacement);
    if (trimmed_phrase.empty()) {
        if (error != nullptr) {
            *error = "phrase cannot be empty";
        }
        return false;
    }
    Entry* target = nullptr;
    for (Entry& entry : entries_) {
        if (entry.id == id) {
            target = &entry;
            break;
        }
    }
    if (target == nullptr) {
        if (error != nullptr) {
            *error = "entry not found";
        }
        return false;
    }
    const std::string key = ToLowerCopy(trimmed_phrase);
    for (const Entry& entry : entries_) {
        if (entry.id != id && ToLowerCopy(TrimCopy(entry.phrase)) == key) {
            if (error != nullptr) {
                *error = "Word already exists.";
            }
            return false;
        }
    }
    if (!trimmed_replacement.empty() &&
        ContainsWholeWord(trimmed_replacement, trimmed_phrase)) {
        if (error != nullptr) {
            *error = "replacement cannot contain the phrase";
        }
        return false;
    }
    target->phrase = trimmed_phrase;
    target->replacement = trimmed_replacement;
    target->has_replacement = !trimmed_replacement.empty();
    if (!target->has_replacement) {
        target->replacement.clear();
    }
    target->updated_at = NowSeconds();
    return true;
}

bool DictionaryStore::Remove(std::int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [id](const Entry& entry) { return entry.id == id; });
    if (found == entries_.end()) {
        return false;
    }
    entries_.erase(found);
    return true;
}

bool DictionaryStore::SetStarred(std::int64_t id, bool starred) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Entry& entry : entries_) {
        if (entry.id == id) {
            entry.starred = starred;
            entry.updated_at = NowSeconds();
            return true;
        }
    }
    return false;
}

bool DictionaryStore::RecordUsage(std::int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Entry& entry : entries_) {
        if (entry.id == id) {
            ++entry.usage_count;
            return true;
        }
    }
    return false;
}

std::string DictionaryStore::ExportJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return EntriesToJson(entries_).dump(4);
}

bool DictionaryStore::ImportJson(const std::string& content, std::string* error) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(content);
    } catch (const nlohmann::json::exception&) {
        if (error != nullptr) {
            *error = "invalid JSON";
        }
        return false;
    }
    std::vector<Entry> parsed;
    bool migrated = false;
    if (!ParseAny(json, NowSeconds(), Source::Imported, parsed, migrated)) {
        if (error != nullptr) {
            *error = "unsupported dictionary format";
        }
        return false;
    }
    if (parsed.size() > kMaxEntries) {
        lwf::Log::Warning("dictionary: truncated to " + std::to_string(kMaxEntries) +
                          " entries on import");
        parsed.resize(kMaxEntries);
    }
    for (const Entry& entry : parsed) {
        if (entry.has_replacement && !entry.replacement.empty() &&
            ContainsWholeWord(entry.replacement, entry.phrase)) {
            if (error != nullptr) {
                *error = "replacement cannot contain the phrase";
            }
            return false;
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::int64_t next = 1;
    for (const Entry& entry : parsed) {
        if (entry.id >= next) {
            next = entry.id + 1;
        }
    }
    entries_ = std::move(parsed);
    next_id_ = next;
    loaded_ = true;
    migrated_ = false;
    bak_pending_ = true;
    return true;
}

bool DictionaryStore::ExportFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream file(tmp_path, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        file << EntriesToJson(entries_).dump(4) << '\n';
        file.flush();
        if (!file.good()) {
            file.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}

bool DictionaryStore::ImportFile(const std::string& path, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error != nullptr) {
            *error = "cannot open import file";
        }
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return ImportJson(stream.str(), error);
}

bool DictionaryStore::Loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

std::size_t DictionaryStore::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

const std::string& DictionaryStore::Path() const {
    return path_;
}
