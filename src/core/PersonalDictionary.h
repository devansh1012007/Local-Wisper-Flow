#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

class PersonalDictionary {
public:
    bool Load(const std::string& path);

    void SetWords(std::vector<std::string> words);
    void SetReplacements(std::vector<std::pair<std::string, std::string>> replacements);
    void AddWord(std::string word);

    std::vector<std::string> Words() const;
    std::vector<std::pair<std::string, std::string>> Replacements() const;
    bool Loaded() const;

    std::string InitialPrompt(std::size_t cap_chars = 600) const;
    std::string Enforce(const std::string& text, bool fuzzy_enabled,
                        std::size_t* fix_count = nullptr) const;
    bool Reload();

    static std::size_t EditDistance(const std::string& a, const std::string& b);

private:
    mutable std::mutex mutex_;
    std::vector<std::string> words_;
    std::vector<std::pair<std::string, std::string>> replacements_;
    std::string path_;
    bool loaded_ = false;
};
