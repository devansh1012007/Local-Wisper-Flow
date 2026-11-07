#include "core/PersonalDictionary.h"
#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

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

bool IsWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c >= 0;
}

struct Token {
    std::size_t begin;
    std::size_t end;
};

std::vector<Token> WordTokens(const std::string& text) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char u = static_cast<unsigned char>(text[i]);
        if (std::isalnum(u) == 0 && u < 0x80) {
            ++i;
            continue;
        }
        const std::size_t begin = i;
        while (i < text.size()) {
            const unsigned char w = static_cast<unsigned char>(text[i]);
            if (std::isalnum(w) != 0 || w >= 0x80) {
                ++i;
            } else {
                break;
            }
        }
        tokens.push_back({begin, i});
    }
    return tokens;
}

bool WholeWordFindInsensitive(const std::string& text, const std::string& needle,
                              std::size_t start, std::size_t& out_begin, std::size_t& out_end) {
    const std::string lowered = ToLowerUtf8Safe(text);
    const std::string target = ToLowerUtf8Safe(needle);
    if (target.empty()) {
        return false;
    }
    std::size_t pos = start;
    for (;;) {
        pos = lowered.find(target, pos);
        if (pos == std::string::npos) {
            return false;
        }
        const std::size_t after = pos + target.size();
        const bool left_ok = pos == 0 || !std::isalnum(static_cast<unsigned char>(text[pos - 1]));
        const bool right_ok =
            after >= text.size() || !std::isalnum(static_cast<unsigned char>(text[after]));
        if (left_ok && right_ok) {
            out_begin = pos;
            out_end = after;
            return true;
        }
        ++pos;
    }
}

}

bool PersonalDictionary::Load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) {
        return false;
    }
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

    std::vector<std::string> words;
    std::vector<std::pair<std::string, std::string>> replacements;

    if (json.is_object() && json.contains("schema_version") &&
        json["schema_version"].is_number() &&
        json["schema_version"].get<std::int64_t>() == 2) {
        if (!json.contains("entries") || !json["entries"].is_array()) {
            return false;
        }
        for (const auto& element : json["entries"]) {
            if (!element.is_object()) {
                continue;
            }
            if (!element.contains("phrase") || !element["phrase"].is_string()) {
                continue;
            }
            const std::string phrase = element["phrase"].get<std::string>();
            words.push_back(phrase);
            if (element.contains("replacement") && element["replacement"].is_string()) {
                const std::string replacement = element["replacement"].get<std::string>();
                if (!replacement.empty()) {
                    replacements.emplace_back(phrase, replacement);
                }
            }
        }
    } else if (json.is_array()) {
        for (const auto& element : json) {
            if (element.is_string()) {
                words.push_back(element.get<std::string>());
            }
        }
    } else if (json.is_object()) {
        if (json.contains("words") && json["words"].is_array()) {
            for (const auto& element : json["words"]) {
                if (element.is_string()) {
                    words.push_back(element.get<std::string>());
                }
            }
        }
        if (json.contains("replacements") && json["replacements"].is_array()) {
            for (const auto& element : json["replacements"]) {
                if (element.is_object() && element.contains("from") && element.contains("to") &&
                    element["from"].is_string() && element["to"].is_string()) {
                    replacements.emplace_back(element["from"].get<std::string>(),
                                              element["to"].get<std::string>());
                }
            }
        }
    } else {
        return false;
    }

    words.erase(std::remove_if(words.begin(), words.end(),
                               [](const std::string& w) {
                                   return w.empty() || w.size() > 60;
                               }),
                words.end());
    words_ = std::move(words);
    replacements_ = std::move(replacements);
    loaded_ = true;
    return true;
}

bool PersonalDictionary::Reload() {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        path = path_;
    }
    if (path.empty()) {
        return false;
    }
    return Load(path);
}

void PersonalDictionary::SetWords(std::vector<std::string> words) {
    std::lock_guard<std::mutex> lock(mutex_);
    words_ = std::move(words);
    loaded_ = true;
}

void PersonalDictionary::SetReplacements(
    std::vector<std::pair<std::string, std::string>> replacements) {
    std::lock_guard<std::mutex> lock(mutex_);
    replacements_ = std::move(replacements);
}

void PersonalDictionary::AddWord(std::string word) {
    const std::string trimmed = [](const std::string& input) {
        const std::size_t begin = input.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return std::string();
        }
        const std::size_t end = input.find_last_not_of(" \t\r\n");
        return input.substr(begin, end - begin + 1);
    }(word);
    if (trimmed.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    words_.erase(std::remove(words_.begin(), words_.end(), trimmed), words_.end());
    words_.insert(words_.begin(), trimmed);
    loaded_ = true;
}

std::vector<std::string> PersonalDictionary::Words() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return words_;
}

std::vector<std::pair<std::string, std::string>> PersonalDictionary::Replacements() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return replacements_;
}

bool PersonalDictionary::Loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}

std::string PersonalDictionary::InitialPrompt(std::size_t cap_chars) const {
    std::vector<std::string> words;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        words = words_;
    }
    if (words.empty()) {
        return {};
    }
    std::string listing = "Glossary: ";
    std::size_t used = 0;
    std::size_t included = 0;
    for (const std::string& word : words) {
        const std::size_t needed = word.size() + (included > 0 ? 2 : 0);
        if (listing.size() + needed > cap_chars) {
            lwf::Log::Warning("personal dictionary prompt truncated at " +
                         std::to_string(included) + " of " +
                         std::to_string(words.size()) + " words");
            break;
        }
        if (included > 0) {
            listing += ", ";
        }
        listing += word;
        used += needed;
        ++included;
    }
    return included > 0 ? listing : std::string();
}

std::size_t PersonalDictionary::EditDistance(const std::string& a, const std::string& b) {
    const std::size_t rows = a.size() + 1;
    const std::size_t cols = b.size() + 1;
    if (rows * cols > 64 * 64) {
        return 999;
    }
    std::vector<std::size_t> previous(cols, 0);
    std::vector<std::size_t> current(cols, 0);
    for (std::size_t c = 0; c < cols; ++c) {
        previous[c] = c;
    }
    for (std::size_t r = 1; r < rows; ++r) {
        current[0] = r;
        for (std::size_t c = 1; c < cols; ++c) {
            const std::size_t substitution = previous[c - 1] + (a[r - 1] == b[c - 1] ? 0 : 1);
            const std::size_t deletion = previous[c] + 1;
            const std::size_t insertion = current[c - 1] + 1;
            current[c] = std::min({substitution, deletion, insertion});
        }
        std::swap(previous, current);
    }
    return previous[cols - 1];
}

std::string PersonalDictionary::Enforce(const std::string& text, bool fuzzy_enabled,
                                        std::size_t* fix_count) const {
    std::size_t local_count = 0;
    std::string out = text;
    std::vector<std::string> words;
    std::vector<std::pair<std::string, std::string>> replacements;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        words = words_;
        replacements = replacements_;
    }

    for (const auto& [from, to] : replacements) {
        if (from.empty()) {
            continue;
        }
        const std::string source = out;
        std::string rebuilt_rule;
        rebuilt_rule.reserve(source.size());
        std::size_t cursor = 0;
        for (;;) {
            std::size_t begin = 0;
            std::size_t end = 0;
            if (!WholeWordFindInsensitive(source, from, cursor, begin, end)) {
                rebuilt_rule += source.substr(cursor);
                break;
            }
            rebuilt_rule += source.substr(cursor, begin - cursor);
            rebuilt_rule += to;
            if (to != source.substr(begin, end - begin)) {
                ++local_count;
            }
            cursor = end;
        }
        out = rebuilt_rule;
    }

    const std::vector<Token> tokens = WordTokens(out);
    std::string rebuilt;
    std::size_t last = 0;
    for (const Token& token : tokens) {
        const std::string original = out.substr(token.begin, token.end - token.begin);
        const std::string lowered = ToLowerUtf8Safe(original);
        std::string replacement_word;

        for (const std::string& word : words) {
            if (ToLowerUtf8Safe(word) == lowered && word != original) {
                replacement_word = word;
                break;
            }
        }

        if (replacement_word.empty() && fuzzy_enabled && original.size() >= 6) {
            for (const std::string& word : words) {
                if (word.size() < 6) {
                    continue;
                }
                if (ToLowerUtf8Safe(word) == lowered) {
                    break;
                }
                if (EditDistance(ToLowerUtf8Safe(word), lowered) == 1) {
                    replacement_word = word;
                    break;
                }
            }
        }

        if (!replacement_word.empty()) {
            ++local_count;
        }
        rebuilt += out.substr(last, token.begin - last);
        rebuilt += replacement_word.empty() ? original : replacement_word;
        last = token.end;
    }
    rebuilt += out.substr(last);
    if (fix_count != nullptr) {
        *fix_count = local_count;
    }
    return rebuilt;
}
