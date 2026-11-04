#include "core/SnippetStore.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

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
    return ToLowerUtf8Safe(trimmed.substr(begin));
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
    if (!json.is_array()) {
        return false;
    }

    std::vector<Entry> entries;
    warnings_.clear();
    for (const auto& element : json) {
        if (!element.is_object() || !element.contains("trigger") ||
            !element.contains("expansion")) {
            continue;
        }
        Entry entry;
        entry.trigger = element["trigger"].get<std::string>();
        entry.expansion = element["expansion"].get<std::string>();
        if (entry.trigger.empty()) {
            continue;
        }
        if (entry.expansion.size() > 4000) {
            warnings_.push_back("snippet '" + entry.trigger + "' rejected: expansion over 4000 chars");
            continue;
        }
        for (const Entry& existing : entries) {
            if (existing.trigger == entry.trigger) {
                warnings_.push_back("duplicate snippet trigger '" + entry.trigger +
                                    "', last wins");
                break;
            }
        }
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [&](const Entry& existing) {
                                         return existing.trigger == entry.trigger;
                                     }),
                      entries.end());
        entries.push_back(std::move(entry));
    }
    for (const std::string& warning : warnings_) {
        lwf::Log::Warning("snippets: " + warning);
    }
    entries_ = std::move(entries);
    loaded_ = true;
    return true;
}

void SnippetStore::SetEntries(std::vector<Entry> entries) {
    entries_ = std::move(entries);
    warnings_.clear();
    loaded_ = true;
}

void SnippetStore::SetMode(std::string mode) {
    mode_ = std::move(mode);
}

const std::vector<SnippetStore::Entry>& SnippetStore::Entries() const {
    return entries_;
}

const std::vector<std::string>& SnippetStore::Warnings() const {
    return warnings_;
}

bool SnippetStore::Loaded() const {
    return loaded_;
}

std::string SnippetStore::Expand(const std::string& text) const {
    if (entries_.empty() || text.empty()) {
        return text;
    }
    const std::string lowered = ToLowerUtf8Safe(text);

    if (mode_ != "contains") {
        const std::string normalized = Normalize(text);
        for (const Entry& entry : entries_) {
            if (Normalize(entry.trigger) == normalized) {
                return entry.expansion;
            }
        }
        return text;
    }

    std::string out = text;
    for (const Entry& entry : entries_) {
        const std::string trigger = ToLowerUtf8Safe(TrimCopy(entry.trigger));
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
            out = out.substr(0, pos) + entry.expansion + out.substr(pos + trigger.size());
            search_from = pos + entry.expansion.size();
        }
    }
    return out;
}
