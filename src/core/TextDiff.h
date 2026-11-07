#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace lwf {

enum class DiffOp {
    Equal,
    Insert,
    Delete,
};

struct DiffToken {
    DiffOp op = DiffOp::Equal;
    std::string text;
};

struct DiffResult {
    std::size_t replaced = 0;
    std::size_t inserted = 0;
    std::size_t deleted = 0;
    std::vector<DiffToken> tokens;
};

DiffResult Diff(const std::string& original, const std::string& edited);

}
