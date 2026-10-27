#include "core/Utf8.h"

namespace {

bool IsContinuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

std::size_t LeadLength(char c) {
    const unsigned char b = static_cast<unsigned char>(c);
    if (b < 0x80) {
        return 1;
    }
    if ((b & 0xE0) == 0xC0) {
        return 2;
    }
    if ((b & 0xF0) == 0xE0) {
        return 3;
    }
    if ((b & 0xF8) == 0xF0) {
        return 4;
    }
    return 0;
}

}  // namespace

namespace Utf8 {

std::size_t TrailingPartialLength(const std::string& text) {
    if (text.empty()) {
        return 0;
    }
    std::size_t look_back = 0;
    while (look_back < 3 && look_back < text.size() &&
           IsContinuation(text[text.size() - 1 - look_back])) {
        ++look_back;
    }
    if (look_back >= text.size()) {
        return 0;
    }
    const char lead = text[text.size() - 1 - look_back];
    const std::size_t lead_length = LeadLength(lead);
    if (lead_length < 2) {
        return 0;
    }
    const std::size_t expected = lead_length - 1;
    if (look_back < expected) {
        return look_back + 1;
    }
    return 0;
}

std::string TrimTrailingPartial(const std::string& text) {
    const std::size_t partial = TrailingPartialLength(text);
    if (partial == 0) {
        return text;
    }
    return text.substr(0, text.size() - partial);
}

std::string NormalizeNewlines(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            out += '\n';
        } else {
            out += text[i];
        }
    }
    return out;
}

std::string NewlinesToCrlf(const std::string& text) {
    const std::string normalized = NormalizeNewlines(text);
    std::string out;
    out.reserve(normalized.size() + 8);
    for (char c : normalized) {
        if (c == '\n') {
            out += "\r\n";
        } else {
            out += c;
        }
    }
    return out;
}

}  // namespace Utf8
