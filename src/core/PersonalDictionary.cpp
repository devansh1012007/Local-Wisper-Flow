#include "core/PersonalDictionary.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    });
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
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           static_cast<unsigned char>(c) >= 0x80;
}

}  // namespace

void PersonalDictionary::Load(const std::string& path) {
    words_.clear();
    replacements_.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[dictionary] failed to parse '" << path << "': " << e.what() << '\n';
        return;
    }

    const auto add_word = [&](const std::string& raw) {
        const std::string word = TrimCopy(raw);
        if (word.empty()) {
            return;
        }
        const std::string folded = ToLowerAscii(word);
        for (const std::string& existing : words_) {
            if (ToLowerAscii(existing) == folded) {
                return;
            }
        }
        words_.push_back(word);
    };

    if (json.is_array()) {
        for (const auto& element : json) {
            if (element.is_string()) {
                add_word(element.get<std::string>());
            }
        }
        return;
    }

    if (!json.is_object()) {
        std::cerr << "[dictionary] '" << path << "' must be an array or an object\n";
        return;
    }

    const auto words = json.find("words");
    if (words != json.end() && words->is_array()) {
        for (const auto& element : *words) {
            if (element.is_string()) {
                add_word(element.get<std::string>());
            }
        }
    }

    const auto replacements = json.find("replacements");
    if (replacements != json.end() && replacements->is_array()) {
        for (const auto& element : *replacements) {
            if (!element.is_object()) {
                continue;
            }
            Replacement entry;
            const auto from = element.find("from");
            const auto to = element.find("to");
            if (from == element.end() || to == element.end() || !from->is_string() ||
                !to->is_string()) {
                std::cerr << "[dictionary] skipping malformed replacement entry\n";
                continue;
            }
            entry.from = from->get<std::string>();
            entry.to = to->get<std::string>();
            if (!entry.from.empty()) {
                replacements_.push_back(std::move(entry));
            }
        }
    }
}

void PersonalDictionary::AddWord(const std::string& word) {
    const std::string trimmed = TrimCopy(word);
    if (trimmed.empty()) {
        return;
    }
    const std::string folded = ToLowerAscii(trimmed);
    for (const std::string& existing : words_) {
        if (ToLowerAscii(existing) == folded) {
            return;
        }
    }
    words_.push_back(trimmed);
}

bool PersonalDictionary::RemoveWord(const std::string& word) {
    const std::string folded = ToLowerAscii(TrimCopy(word));
    for (std::size_t i = 0; i < words_.size(); ++i) {
        if (ToLowerAscii(words_[i]) == folded) {
            words_.erase(words_.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

std::string PersonalDictionary::PromptFragment() const {
    truncated_last_prompt_ = false;
    if (words_.empty()) {
        return {};
    }
    std::string fragment = "Custom vocabulary:";
    for (const std::string& word : words_) {
        const std::string piece = " " + word + ",";
        if (fragment.size() + piece.size() > kPromptCharCap) {
            truncated_last_prompt_ = true;
            break;
        }
        fragment += piece;
    }
    fragment.back() = '.';
    return fragment;
}

std::string PersonalDictionary::Enforce(const std::string& text) const {
    if (replacements_.empty() || text.empty()) {
        return text;
    }
    std::string out = text;
    for (const Replacement& replacement : replacements_) {
        const std::string folded_from = ToLowerAscii(replacement.from);
        if (folded_from.empty()) {
            continue;
        }
        const std::string folded_out = ToLowerAscii(out);
        std::string result;
        result.reserve(out.size());
        std::size_t i = 0;
        while (i < out.size()) {
            if (folded_out.compare(i, folded_from.size(), folded_from) == 0) {
                const bool left_ok = i == 0 || !IsWordChar(out[i - 1]);
                const std::size_t end = i + folded_from.size();
                const bool right_ok = end >= out.size() || !IsWordChar(out[end]);
                if (left_ok && right_ok) {
                    result += replacement.to;
                    i = end;
                    continue;
                }
            }
            result += out[i];
            ++i;
        }
        out = std::move(result);
    }
    return out;
}
