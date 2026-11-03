#include "core/SpokenCommandProcessor.h"

#include <cctype>
#include <algorithm>
#include <sstream>
#include <vector>

namespace {

struct Command {
    const char* phrase;
    const char* sentinel;
};

const Command kCommands[] = {
    {"new line", "LWF_NL"},
    {"new paragraph", "LWF_PAR"},
    {"period", "LWF_DOT"},
    {"full stop", "LWF_DOT"},
    {"comma", "LWF_COMMA"},
    {"question mark", "LWF_QM"},
    {"exclamation mark", "LWF_EXCL"},
    {"exclamation point", "LWF_EXCL"},
    {"colon", "LWF_COLON"},
    {"semicolon", "LWF_SEMI"},
    {"open quote", "LWF_OQUOTE"},
    {"close quote", "LWF_CQUOTE"},
    {"bullet point", "LWF_BULLET"},
    {"scratch that", "LWF_SCRATCH"},
};

std::string LowerAscii(std::string text) {
    for (char& c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) {
            c = static_cast<char>(std::tolower(u));
        }
    }
    return text;
}

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

bool IsPunctuation(char c) {
    return c == ',' || c == '.' || c == '!' || c == '?' || c == ';' || c == ':';
}

std::vector<std::string> SplitSegments(const std::string& text) {
    std::vector<std::string> segments;
    std::string current;
    for (const char c : text) {
        if (IsPunctuation(c)) {
            current.push_back(c);
            segments.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        segments.push_back(std::move(current));
    }
    return segments;
}

bool WordEquals(const std::string& text, std::size_t begin, std::size_t end,
                const std::string& phrase) {
    if (end <= begin || end - begin != phrase.size()) {
        return false;
    }
    for (std::size_t i = 0; i < phrase.size(); ++i) {
        const unsigned char u = static_cast<unsigned char>(text[begin + i]);
        if (u < 0x80) {
            if (static_cast<char>(std::tolower(u)) != phrase[i]) {
                return false;
            }
        } else {
            if (text[begin + i] != phrase[i]) {
                return false;
            }
        }
    }
    return true;
}

std::size_t WholeWordFind(const std::string& text, const std::string& phrase,
                          std::size_t from) {
    if (phrase.empty()) {
        return std::string::npos;
    }
    const std::string lowered = LowerAscii(text);
    std::size_t pos = from;
    for (;;) {
        pos = lowered.find(phrase, pos);
        if (pos == std::string::npos) {
            return std::string::npos;
        }
        const auto word_char = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        };
        const bool left_ok = pos == 0 || !word_char(lowered[pos - 1]);
        const std::size_t after = pos + phrase.size();
        const bool right_ok = after >= lowered.size() || !word_char(lowered[after]);
        if (left_ok && right_ok) {
            return pos;
        }
        ++pos;
    }
}

}

void SpokenCommandProcessor::SetAggressive(bool aggressive) {
    aggressive_ = aggressive;
}

bool SpokenCommandProcessor::Aggressive() const {
    return aggressive_;
}

std::string SpokenCommandProcessor::ToSentinels(const std::string& raw) const {
    if (!aggressive_) {
        std::string out;
        const std::vector<std::string> segments = SplitSegments(raw);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) {
                out += ' ';
            }
            std::string trimmed = TrimCopy(LowerAscii(segments[i]));
            while (!trimmed.empty() && IsPunctuation(trimmed.back())) {
                trimmed.pop_back();
            }
            trimmed = TrimCopy(trimmed);
            std::string replacement;
            for (const Command& command : kCommands) {
                if (trimmed == std::string(command.phrase)) {
                    replacement = command.sentinel;
                    break;
                }
            }
            if (replacement.empty()) {
                out += TrimCopy(segments[i]);
            } else {
                out += replacement;
            }
        }
        return TrimCopy(out);
    }

    std::string out = raw;
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Command& command : kCommands) {
            const std::string phrase = command.phrase;
            const std::size_t pos = WholeWordFind(out, phrase, 0);
            if (pos != std::string::npos) {
                out.replace(pos, phrase.size(), command.sentinel);
                changed = true;
            }
        }
    }
    return out;
}

std::string SpokenCommandProcessor::ExpandSentinels(const std::string& text) const {
    std::string out = text;
    for (;;) {
        const std::size_t scratch = out.find("LWF_SCRATCH");
        if (scratch != std::string::npos) {
            out.erase(0, scratch + std::string("LWF_SCRATCH").size());
            continue;
        }
        bool changed = false;
        for (const Command& command : kCommands) {
            const std::string sentinel = command.sentinel;
            const std::size_t pos = out.find(sentinel);
            if (pos == std::string::npos) {
                continue;
            }
            std::string expansion;
            bool attach_to_previous = false;
            bool attach_to_next = false;
            bool ensure_space_after = false;
            if (sentinel == std::string("LWF_NL")) {
                expansion = "\n";
            } else if (sentinel == std::string("LWF_PAR")) {
                expansion = "\n\n";
            } else if (sentinel == std::string("LWF_DOT")) {
                expansion = ".";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_COMMA")) {
                expansion = ",";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_QM")) {
                expansion = "?";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_EXCL")) {
                expansion = "!";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_COLON")) {
                expansion = ":";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_SEMI")) {
                expansion = ";";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_OQUOTE")) {
                expansion = "\"";
                attach_to_next = true;
            } else if (sentinel == std::string("LWF_CQUOTE")) {
                expansion = "\"";
                attach_to_previous = true;
            } else if (sentinel == std::string("LWF_BULLET")) {
                expansion = "-";
                ensure_space_after = true;
            }
            out.replace(pos, sentinel.size(), expansion);
            if (ensure_space_after) {
                const std::size_t after = pos + expansion.size();
                if (after >= out.size() || out[after] != ' ') {
                    out.insert(after, " ");
                }
            }
            if (attach_to_previous) {
                std::size_t erase_pos = pos;
                while (erase_pos > 0 && out[erase_pos - 1] == ' ') {
                    --erase_pos;
                }
                if (erase_pos < pos) {
                    out.erase(erase_pos, pos - erase_pos);
                }
            }
            if (attach_to_next) {
                std::size_t erase_pos = pos + expansion.size();
                while (erase_pos < out.size() && out[erase_pos] == ' ') {
                    ++erase_pos;
                }
                out.erase(pos + expansion.size(), erase_pos - (pos + expansion.size()));
            }
            changed = true;
        }
        if (!changed) {
            break;
        }
    }
    std::string cleaned;
    cleaned.reserve(out.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        const char c = out[i];
        if (c == '\n') {
            while (!cleaned.empty() && cleaned.back() == ' ') {
                cleaned.pop_back();
            }
            cleaned.push_back('\n');
            while (i + 1 < out.size() && out[i + 1] == ' ') {
                ++i;
            }
            continue;
        }
        cleaned.push_back(c);
    }
    return TrimCopy(cleaned);
}

std::size_t SpokenCommandProcessor::CountSentinels(const std::string& text) {
    std::vector<std::string> unique;
    for (const Command& command : kCommands) {
        if (std::find(unique.begin(), unique.end(), std::string(command.sentinel)) ==
            unique.end()) {
            unique.push_back(command.sentinel);
        }
    }
    std::size_t count = 0;
    for (const std::string& sentinel : unique) {
        std::size_t pos = text.find(sentinel);
        while (pos != std::string::npos) {
            ++count;
            pos = text.find(sentinel, pos + 1);
        }
    }
    return count;
}

std::string SpokenCommandProcessor::SentinelPreservationHint() {
    return "Tokens like LWF_NL, LWF_DOT or LWF_COMMA are formatting markers. "
           "Keep every such token exactly as written, never translate or expand them.";
}
