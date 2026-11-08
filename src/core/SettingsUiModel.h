#pragma once

#include <string>
#include <vector>

enum class ApplyClass { HotApply, RestartRequired };

class SettingsStore;

namespace SettingsUiModel {

struct SettingDiff {
    std::vector<std::string> hot_keys;
    std::vector<std::string> restart_keys;
};

SettingDiff ClassifyDiff(const SettingsStore& before, const SettingsStore& after);

std::string ValidateInt(const std::string& key, int value, int min_value, int max_value);
std::string ValidateModelPath(const std::string& path);
std::string ValidateLanguage(const std::string& code);

// Microphone picker index contract (Settings > Speech). The combo entry list
// is built as entry 0 = "System default" (stored value "") followed by the
// capture-device display names in enumeration order, so this returns the
// 0-based index into THAT combined list:
//   selected == ""            -> 0 (System default)
//   selected == names[i]      -> i + 1
//   unknown / stale name      -> 0 (falls back to System default)
//   empty list + empty select -> 0
// The caller derives the label from the index: index 0 renders the fixed
// "System default" string, otherwise names[index - 1].
int MatchMicDeviceIndex(const std::vector<std::string>& names, const std::string& selected);

// API key entry helpers (Settings UI). Pure logic only: trim, source
// derivation from live store/env state, and user-facing status labels.
// Key material never crosses these functions; callers keep the secret in
// a masked buffer and persist it through ApiKeyStore directly. Labels are
// sentence-case and avoid internal terms per the F9 copy lint.
enum class ApiKeySource { Saved, Environment, NotSet };

ApiKeySource ApiKeySourceFor(bool has_saved, bool has_env);
const char* ApiKeySourceLabel(ApiKeySource source);
const char* ApiKeyStatusWord(ApiKeySource source);
std::string TrimApiKeyInput(const std::string& raw);
// Returns the inline error for a trimmed value, empty when save may proceed.
std::string ApiKeySaveError(const std::string& trimmed);

class DictionaryEditorModel {
public:
    struct Word {
        std::string value;
    };
    struct Replacement {
        std::string from;
        std::string to;
    };

    std::vector<Word> words;
    std::vector<Replacement> replacements;

    void AddWord(std::string value);
    void RemoveWord(std::size_t index);
    void AddReplacement(std::string from, std::string to);
    void RemoveReplacement(std::size_t index);
    std::size_t Dedupe();
};

class SnippetEditorModel {
public:
    struct Row {
        std::string trigger;
        std::string expansion;
    };

    std::vector<Row> rows;

    void Add(std::string trigger, std::string expansion);
    void Remove(std::size_t index);
    std::size_t Dedupe();
    bool Valid(std::string& error) const;
};

}
