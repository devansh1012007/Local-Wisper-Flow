#include "core/InjectorDecorators.h"

#include <algorithm>

namespace {

bool IsBreakable(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

}

std::vector<std::string> ChunkedInjector::Split(const std::string& text,
                                                std::size_t max_chunk_chars) {
    if (max_chunk_chars == 0) {
        max_chunk_chars = 1;
    }
    if (text.size() <= max_chunk_chars) {
        if (text.empty()) {
            return {};
        }
        return {text};
    }

    std::vector<std::string> chunks;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t remaining = text.size() - start;
        if (remaining <= max_chunk_chars) {
            chunks.push_back(text.substr(start));
            break;
        }
        std::size_t cut = start + max_chunk_chars;
        std::size_t search = cut;
        bool found = false;
        while (search > start) {
            --search;
            if (IsBreakable(text[search])) {
                cut = search + 1;
                found = true;
                break;
            }
        }
        if (!found) {
            cut = start + max_chunk_chars;
        }
        chunks.push_back(text.substr(start, cut - start));
        start = cut;
    }
    return chunks;
}

ChunkedInjector::ChunkedInjector(ITextInjector& inner, std::size_t max_chunk_chars)
    : inner_(inner), max_chunk_chars_(max_chunk_chars) {}

void ChunkedInjector::InjectText(const std::string& text) {
    for (const std::string& chunk : Split(text, max_chunk_chars_)) {
        inner_.InjectText(chunk);
    }
}
