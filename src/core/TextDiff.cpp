#include "core/TextDiff.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace lwf {
namespace {

bool IsSpaceCodePoint(std::uint32_t cp) {
    if (cp >= 0x09 && cp <= 0x0D) {
        return true;
    }
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) {
        return true;
    }
    if (cp >= 0x2000 && cp <= 0x200A) {
        return true;
    }
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

std::size_t Utf8SequenceLength(unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

std::uint32_t DecodeCodePoint(const std::string& text, std::size_t pos, std::size_t& length) {
    const auto lead = static_cast<unsigned char>(text[pos]);
    length = Utf8SequenceLength(lead);
    if (pos + length > text.size()) {
        length = 1;
        return lead;
    }
    std::uint32_t cp = lead & (length == 1 ? 0x7F : (0x7F >> length));
    for (std::size_t k = 1; k < length; ++k) {
        const auto cont = static_cast<unsigned char>(text[pos + k]);
        if ((cont & 0xC0) != 0x80) {
            length = 1;
            return lead;
        }
        cp = (cp << 6) | (cont & 0x3F);
    }
    return cp;
}

std::vector<std::string> Tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::size_t start = std::string::npos;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t length = 0;
        const std::uint32_t cp = DecodeCodePoint(text, pos, length);
        if (IsSpaceCodePoint(cp)) {
            if (start != std::string::npos) {
                tokens.push_back(text.substr(start, pos - start));
                start = std::string::npos;
            }
        } else if (start == std::string::npos) {
            start = pos;
        }
        pos += length;
    }
    if (start != std::string::npos) {
        tokens.push_back(text.substr(start, pos - start));
    }
    return tokens;
}

}

DiffResult Diff(const std::string& original, const std::string& edited) {
    const std::vector<std::string> a = Tokenize(original);
    const std::vector<std::string> b = Tokenize(edited);
    const std::size_t n = a.size();
    const std::size_t m = b.size();

    std::vector<std::vector<std::size_t>> table(n + 1, std::vector<std::size_t>(m + 1, 0));
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            if (a[i - 1] == b[j - 1]) {
                table[i][j] = table[i - 1][j - 1] + 1;
            } else {
                table[i][j] = std::max(table[i - 1][j], table[i][j - 1]);
            }
        }
    }

    std::vector<DiffToken> reversed;
    std::size_t i = n;
    std::size_t j = m;
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            reversed.push_back(DiffToken{DiffOp::Equal, a[i - 1]});
            --i;
            --j;
        } else if (table[i - 1][j] > table[i][j - 1]) {
            reversed.push_back(DiffToken{DiffOp::Delete, a[i - 1]});
            --i;
        } else {
            reversed.push_back(DiffToken{DiffOp::Insert, b[j - 1]});
            --j;
        }
    }
    while (i > 0) {
        reversed.push_back(DiffToken{DiffOp::Delete, a[i - 1]});
        --i;
    }
    while (j > 0) {
        reversed.push_back(DiffToken{DiffOp::Insert, b[j - 1]});
        --j;
    }

    DiffResult result;
    result.tokens.reserve(reversed.size());
    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) {
        result.tokens.push_back(std::move(*it));
    }

    std::size_t raw_inserted = 0;
    std::size_t raw_deleted = 0;
    for (const DiffToken& token : result.tokens) {
        if (token.op == DiffOp::Insert) {
            ++raw_inserted;
        } else if (token.op == DiffOp::Delete) {
            ++raw_deleted;
        }
    }
    result.replaced = std::min(raw_inserted, raw_deleted);
    result.inserted = raw_inserted - result.replaced;
    result.deleted = raw_deleted - result.replaced;
    return result;
}

}
