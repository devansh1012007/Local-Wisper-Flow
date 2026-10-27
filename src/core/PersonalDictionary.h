#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

class PersonalDictionary {
public:
    struct Replacement {
        std::string from;
        std::string to;
    };

    static constexpr std::size_t kPromptCharCap = 600;

    void Load(const std::string& path);

    void AddWord(const std::string& word);
    bool RemoveWord(const std::string& word);

    std::string PromptFragment() const;

    std::string Enforce(const std::string& text) const;

    const std::vector<std::string>& Words() const {
        return words_;
    }

    const std::vector<Replacement>& Replacements() const {
        return replacements_;
    }

    bool TruncatedLastPrompt() const {
        return truncated_last_prompt_;
    }

private:
    std::vector<std::string> words_;
    std::vector<Replacement> replacements_;
    mutable bool truncated_last_prompt_ = false;
};
