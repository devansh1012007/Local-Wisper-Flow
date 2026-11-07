#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
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

struct TransformDiff {
    std::string before;
    std::string after;
    DiffResult diff;
    std::int64_t captured_ms = 0;
    bool valid = false;
};

class TransformDiffStore {
public:
    static constexpr std::int64_t kRetentionMs = 40000;

    void Put(const std::string& before, const std::string& after, std::int64_t captured_ms);
    TransformDiff Get(std::int64_t now_ms) const;
    bool Has(std::int64_t now_ms) const;

private:
    mutable std::mutex mutex_;
    TransformDiff current_;
};

}
