#include "core/WhisperText.h"

#include <algorithm>

namespace {

const char* const kHallucinationMarkers[] = {
    "[blank_audio]",
    "[silence]",
    "[inaudible]",
    "[noise]",
    "[music]",
    "(music)",
    "(no speech)",
    "(silence)",
    "(inaudible)",
    "(noise)",
    "(blank audio)",
    "(music fades)",
    "(soft music)",
    "(upbeat music)",
    "..."
};

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    });
    return out;
}

bool MatchesMarker(const std::string& candidate, const char* marker) {
    return candidate == marker;
}

std::string StripSurroundingBrackets(std::string text) {
    while (!text.empty() && (text.front() == '[' || text.front() == '(' || text.front() == '.' ||
                             text.front() == ' ')) {
        text.erase(0, 1);
    }
    while (!text.empty() && (text.back() == ']' || text.back() == ')' || text.back() == '.' ||
                             text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

}  // namespace

namespace WhisperText {

bool IsOnlyHallucination(const std::string& transcript) {
    std::string candidate = ToLowerAscii(TrimCopy(transcript));
    if (candidate.empty()) {
        return true;
    }
    for (const char* marker : kHallucinationMarkers) {
        if (MatchesMarker(candidate, marker)) {
            return true;
        }
    }
    candidate = StripSurroundingBrackets(candidate);
    for (const char* marker : kHallucinationMarkers) {
        if (MatchesMarker(candidate, marker)) {
            return true;
        }
    }
    return false;
}

std::string FilterHallucinations(const std::string& transcript) {
    if (IsOnlyHallucination(transcript)) {
        return {};
    }
    return TrimCopy(transcript);
}

}  // namespace WhisperText
