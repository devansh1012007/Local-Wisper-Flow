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
