#pragma once

#include <cstddef>
#include <string>

namespace lwf {

inline std::string RedactSecret(const std::string& text, const std::string& secret) {
    if (text.empty() || secret.empty()) {
        return text;
    }
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t hit = text.find(secret, pos);
        if (hit == std::string::npos) {
            out.append(text, pos, std::string::npos);
            break;
        }
        out.append(text, pos, hit - pos);
        out += "[redacted]";
        pos = hit + secret.size();
    }
    return out;
}

inline bool IsSensitiveHeaderName(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (const char c : name) {
        const char folded = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        lower.push_back(folded);
    }
    return lower == "authorization" || lower == "proxy-authorization" ||
           lower == "xi-api-key" || lower == "x-api-key-id" || lower == "cookie";
}

inline std::string RedactAuthValues(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t stop = text.find('\n', pos);
        stop = stop == std::string::npos ? text.size() : stop + 1;
        const std::string line = text.substr(pos, stop - pos);
        const std::size_t colon = line.find(':');
        bool sensitive = false;
        if (colon != std::string::npos) {
            std::size_t end = colon;
            while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t')) {
                --end;
            }
            std::size_t begin = end;
            while (begin > 0 && line[begin - 1] != '\r' && line[begin - 1] != '\n') {
                --begin;
            }
            sensitive = IsSensitiveHeaderName(line.substr(begin, end - begin));
        }
        if (sensitive) {
            out += line.substr(0, colon + 1);
            out += " [redacted]";
            std::size_t tail = line.size();
            while (tail > colon + 1 && (line[tail - 1] == '\n' || line[tail - 1] == '\r')) {
                --tail;
            }
            out += line.substr(tail);
        } else {
            out += line;
        }
        pos = stop;
    }
    return out;
}

}
