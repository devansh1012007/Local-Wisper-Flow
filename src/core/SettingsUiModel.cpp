#include "core/SettingsUiModel.h"
#include "core/Languages.h"
#include "core/SettingsStore.h"

#include <algorithm>
#include <set>

namespace SettingsUiModel {

SettingDiff ClassifyDiff(const SettingsStore& before, const SettingsStore& after) {
    SettingDiff diff;
    for (const SettingsStore::SchemaEntry& entry : SettingsStore::Schema()) {
        const std::string key = entry.key;
        bool changed = false;
        switch (entry.type) {
            case SettingsStore::Type::Int:
                changed = before.GetInt(key) != after.GetInt(key);
                break;
            case SettingsStore::Type::Double:
                changed = before.GetDouble(key) != after.GetDouble(key);
                break;
            case SettingsStore::Type::Bool:
                changed = before.GetBool(key) != after.GetBool(key);
                break;
            case SettingsStore::Type::String:
                changed = before.GetString(key) != after.GetString(key);
                break;
        }
        if (!changed) {
            continue;
        }
        if (entry.hot_apply) {
            diff.hot_keys.push_back(key);
        } else {
            diff.restart_keys.push_back(key);
        }
    }
    return diff;
}

std::string ValidateInt(const std::string& key, int value, int min_value, int max_value) {
    if (value < min_value || value > max_value) {
        return key + " must be between " + std::to_string(min_value) + " and " +
               std::to_string(max_value);
    }
    return {};
}

std::string ValidateModelPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    if (path.size() > 500) {
        return "model path is too long";
    }
    return {};
}

std::string ValidateLanguage(const std::string& code) {
    if (!Languages::IsSupported(code)) {
        return "unsupported language code: " + code;
    }
    return {};
}

int MatchMicDeviceIndex(const std::vector<std::string>& names, const std::string& selected) {
    // INVARIANT: entry 0 of the picker is always "System default" (stored
    // value ""), so a stored value that is empty or no longer in the list
    // resolves to 0 and the caller shows the default entry.
    if (selected.empty()) {
        return 0;
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == selected) {
            return static_cast<int>(i + 1);
        }
    }
    return 0;
}

int MatchModelIndex(const std::vector<std::string>& fetched, const std::string& selected) {
    // INVARIANT: no sentinel entry exists in this combo, so "no match" is -1
    // and the caller layers its synthetic entry on top of this result.
    if (selected.empty()) {
        return -1;
    }
    for (std::size_t i = 0; i < fetched.size(); ++i) {
        if (fetched[i] == selected) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

ApiKeySource ApiKeySourceFor(bool has_saved, bool has_env) {
    if (has_saved) {
        return ApiKeySource::Saved;
    }
    if (has_env) {
        return ApiKeySource::Environment;
    }
    return ApiKeySource::NotSet;
}

const char* ApiKeySourceLabel(ApiKeySource source) {
    switch (source) {
        case ApiKeySource::Saved:
            return "Saved key";
        case ApiKeySource::Environment:
            return "Environment key";
        case ApiKeySource::NotSet:
            return "Not set";
    }
    return "Not set";
}

const char* ApiKeyStatusWord(ApiKeySource source) {
    switch (source) {
        case ApiKeySource::Saved:
            return "Ready";
        case ApiKeySource::Environment:
            return "Ready";
        case ApiKeySource::NotSet:
            return "Not set";
    }
    return "Not set";
}

std::string TrimApiKeyInput(const std::string& raw) {
    const std::size_t first = raw.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = raw.find_last_not_of(" \t\r\n");
    return raw.substr(first, last - first + 1);
}

std::string ApiKeySaveError(const std::string& trimmed) {
    if (trimmed.empty()) {
        return "Enter a key to save.";
    }
    return {};
}

void DictionaryEditorModel::AddWord(std::string value) {
    if (!value.empty()) {
        words.push_back({std::move(value)});
    }
}

void DictionaryEditorModel::RemoveWord(std::size_t index) {
    if (index < words.size()) {
        words.erase(words.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void DictionaryEditorModel::AddReplacement(std::string from, std::string to) {
    if (!from.empty() && !to.empty()) {
        replacements.push_back({std::move(from), std::move(to)});
    }
}

void DictionaryEditorModel::RemoveReplacement(std::size_t index) {
    if (index < replacements.size()) {
        replacements.erase(replacements.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

std::size_t DictionaryEditorModel::Dedupe() {
    std::set<std::string> seen_words;
    words.erase(std::remove_if(words.begin(), words.end(),
                               [&seen_words](const Word& word) {
                                   return !seen_words.insert(word.value).second;
                               }),
                words.end());
    std::set<std::pair<std::string, std::string>> seen_pairs;
    const auto key_of = [](const Replacement& r) { return std::make_pair(r.from, r.to); };
    replacements.erase(std::remove_if(replacements.begin(), replacements.end(),
                                      [&seen_pairs, &key_of](const Replacement& r) {
                                          return !seen_pairs.insert(key_of(r)).second;
                                      }),
                       replacements.end());
    return seen_words.size() + seen_pairs.size();
}

void SnippetEditorModel::Add(std::string trigger, std::string expansion) {
    if (!trigger.empty()) {
        rows.push_back({std::move(trigger), std::move(expansion)});
    }
}

void SnippetEditorModel::Remove(std::size_t index) {
    if (index < rows.size()) {
        rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

std::size_t SnippetEditorModel::Dedupe() {
    std::set<std::string> seen;
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [&seen](const Row& row) {
                                  return !seen.insert(row.trigger).second;
                              }),
               rows.end());
    return rows.size();
}

bool SnippetEditorModel::Valid(std::string& error) const {
    for (const Row& row : rows) {
        if (row.trigger.empty()) {
            error = "snippet trigger cannot be empty";
            return false;
        }
        if (row.expansion.size() > 4000) {
            error = "snippet '" + row.trigger + "' expansion exceeds 4000 characters";
            return false;
        }
    }
    return true;
}

}
