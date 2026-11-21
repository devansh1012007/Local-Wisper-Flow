#include "core/WhisperText.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

const char* const kBracketMarkers[] = {
    "[blank_audio]",
    "[silence]",
    "[inaudible]",
    "[noise]",
    "[music]",
};

const char* const kSoftMarkers[] = {
    "(music)",
    "(no speech)",
    "(silence)",
    "(inaudible)",
    "(noise)",
    "(blank audio)",
    "(music fades)",
    "(soft music)",
    "(upbeat music)",
    "...",
};

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    });
    return out;
}

bool IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string RemoveBracketMarkers(const std::string& input) {
    const std::string lower = ToLowerAscii(input);
    std::string out;
    out.reserve(input.size());
    std::size_t i = 0;
    while (i < input.size()) {
        bool matched = false;
        for (const char* marker : kBracketMarkers) {
            const std::string needle(marker);
            const std::size_t len = needle.size();
            if (i + len <= lower.size() && lower.compare(i, len, needle) == 0) {
                out.push_back(' ');
                i += len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            out.push_back(input[i]);
            ++i;
        }
    }
    return out;
}

std::vector<std::string> SplitByWhitespace(const std::string& input) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && IsWhitespace(input[i])) {
            ++i;
        }
        if (i >= input.size()) {
            break;
        }
        const std::size_t begin = i;
        while (i < input.size() && !IsWhitespace(input[i])) {
            ++i;
        }
        tokens.push_back(input.substr(begin, i - begin));
    }
    return tokens;
}

std::vector<std::string> SplitBySpace(const std::string& input) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (begin < input.size()) {
        const std::size_t space = input.find(' ', begin);
        if (space == std::string::npos) {
            parts.push_back(input.substr(begin));
            break;
        }
        parts.push_back(input.substr(begin, space - begin));
        begin = space + 1;
    }
    return parts;
}

bool IsPunctuationOnly(const std::string& token) {
    if (token.empty()) {
        return true;
    }
    for (char ch : token) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c >= 0x80) {
            return false;
        }
    }
    return true;
}

std::string ComparisonKey(const std::string& token) {
    const std::string lower = ToLowerAscii(token);
    if (lower.size() >= 3) {
        bool allDots = true;
        for (char c : lower) {
            if (c != '.') {
                allDots = false;
                break;
            }
        }
        if (allDots) {
            return "...";
        }
    }
    std::size_t end = lower.size();
    while (end > 0) {
        const char c = lower[end - 1];
        if (c == ',' || c == ';' || c == ':' || c == '!' || c == '?') {
            --end;
        } else {
            break;
        }
    }
    if (end >= 3) {
        bool allDots = true;
        for (std::size_t i = 0; i < end; ++i) {
            if (lower[i] != '.') {
                allDots = false;
                break;
            }
        }
        if (allDots) {
            return "...";
        }
    }
    while (end > 0) {
        const char c = lower[end - 1];
        if (c == ',' || c == '.' || c == ';' || c == ':' || c == '!' || c == '?') {
            --end;
        } else {
            break;
        }
    }
    return lower.substr(0, end);
}

std::string NormalizeCrLf(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r' && i + 1 < input.size() && input[i + 1] == '\n') {
            out.push_back('\n');
            ++i;
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

std::vector<std::string> SplitLines(const std::string& input) {
    std::vector<std::string> lines;
    if (input.empty()) {
        lines.push_back({});
        return lines;
    }
    std::size_t begin = 0;
    while (begin < input.size()) {
        const std::size_t pos = input.find('\n', begin);
        if (pos == std::string::npos) {
            lines.push_back(input.substr(begin));
            break;
        }
        lines.push_back(input.substr(begin, pos - begin));
        begin = pos + 1;
        if (begin == input.size()) {
            lines.push_back({});
            break;
        }
    }
    return lines;
}

struct Span {
    std::size_t start;
    std::size_t end;
};

std::string FilterLine(const std::string& transcript) {
    const std::string withoutBrackets = RemoveBracketMarkers(transcript);
    std::vector<std::string> tokens = SplitByWhitespace(withoutBrackets);
    if (tokens.empty()) {
        return {};
    }
    std::vector<std::string> keys;
    keys.reserve(tokens.size());
    for (const std::string& token : tokens) {
        keys.push_back(ComparisonKey(token));
    }
    std::vector<std::vector<std::string>> markerSequences;
    markerSequences.reserve(10);
    for (const char* marker : kSoftMarkers) {
        markerSequences.push_back(SplitBySpace(marker));
    }
    std::vector<Span> spans;
    std::vector<bool> inMarker(tokens.size(), false);
    std::size_t i = 0;
    while (i < tokens.size()) {
        std::size_t bestLen = 0;
        for (const std::vector<std::string>& seq : markerSequences) {
            const std::size_t len = seq.size();
            if (len == 0 || len < bestLen) {
                continue;
            }
            if (i + len > tokens.size()) {
                continue;
            }
            bool match = true;
            for (std::size_t k = 0; k < len; ++k) {
                if (keys[i + k] != seq[k]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                bestLen = len;
            }
        }
        if (bestLen > 0) {
            const std::size_t start = i;
            const std::size_t end = i + bestLen - 1;
            spans.push_back(Span{start, end});
            for (std::size_t k = start; k <= end; ++k) {
                inMarker[k] = true;
            }
            i += bestLen;
        } else {
            ++i;
        }
    }
    std::vector<bool> removed(tokens.size(), false);
    for (const Span& span : spans) {
        const bool hasLeft = span.start > 0;
        const bool hasRight = span.end + 1 < tokens.size();
        bool leftReal = false;
        bool rightReal = false;
        if (hasLeft) {
            const std::size_t leftIdx = span.start - 1;
            leftReal = !inMarker[leftIdx] && !IsPunctuationOnly(tokens[leftIdx]);
        }
        if (hasRight) {
            const std::size_t rightIdx = span.end + 1;
            rightReal = !inMarker[rightIdx] && !IsPunctuationOnly(tokens[rightIdx]);
        }
        const bool surroundedByReal = hasLeft && hasRight && leftReal && rightReal;
        if (!surroundedByReal) {
            for (std::size_t k = span.start; k <= span.end; ++k) {
                removed[k] = true;
            }
        }
    }
    std::vector<std::string> survivors;
    survivors.reserve(tokens.size());
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        if (removed[k]) {
            continue;
        }
        if (!inMarker[k] && IsPunctuationOnly(tokens[k])) {
            continue;
        }
        survivors.push_back(tokens[k]);
    }
    std::string out;
    for (std::size_t k = 0; k < survivors.size(); ++k) {
        if (k > 0) {
            out.push_back(' ');
        }
        out += survivors[k];
    }
    return out;
}

}  // namespace

namespace WhisperText {

std::string FilterHallucinations(const std::string& transcript) {
    const std::string normalized = NormalizeCrLf(transcript);
    const std::vector<std::string> lines = SplitLines(normalized);
    std::vector<std::string> kept;
    kept.reserve(lines.size());
    for (const std::string& line : lines) {
        if (SplitByWhitespace(line).empty()) {
            kept.push_back({});
        } else {
            const std::string filtered = FilterLine(line);
            if (!filtered.empty()) {
                kept.push_back(filtered);
            }
        }
    }
    std::string out;
    for (std::size_t i = 0; i < kept.size(); ++i) {
        if (i > 0) {
            out.push_back('\n');
        }
        out += kept[i];
    }
    bool hasContent = false;
    for (char c : out) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            hasContent = true;
            break;
        }
    }
    if (!hasContent) {
        return {};
    }
    return out;
}

bool IsOnlyHallucination(const std::string& transcript) {
    return FilterHallucinations(transcript).empty();
}

}  // namespace WhisperText
