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

struct Span {
    std::size_t start;
    std::size_t end;
};

std::string FilterImpl(const std::string& transcript) {
    const std::string withoutBrackets = RemoveBracketMarkers(transcript);
    std::vector<std::string> tokens = SplitByWhitespace(withoutBrackets);
    if (tokens.empty()) {
        return {};
    }
    std::vector<std::string> lowerTokens;
    lowerTokens.reserve(tokens.size());
    for (const std::string& token : tokens) {
        lowerTokens.push_back(ToLowerAscii(token));
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
                if (lowerTokens[i + k] != seq[k]) {
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
    return FilterImpl(transcript);
}

bool IsOnlyHallucination(const std::string& transcript) {
    return FilterImpl(transcript).empty();
}

}  // namespace WhisperText
