#include "core/TextDiff.h"

#include <doctest/doctest.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool HasInsert(const lwf::DiffResult& result, const std::string& text) {
    for (const lwf::DiffToken& token : result.tokens) {
        if (token.op == lwf::DiffOp::Insert && token.text == text) {
            return true;
        }
    }
    return false;
}

bool HasDelete(const lwf::DiffResult& result, const std::string& text) {
    for (const lwf::DiffToken& token : result.tokens) {
        if (token.op == lwf::DiffOp::Delete && token.text == text) {
            return true;
        }
    }
    return false;
}

std::size_t CountOp(const lwf::DiffResult& result, lwf::DiffOp op) {
    std::size_t count = 0;
    for (const lwf::DiffToken& token : result.tokens) {
        if (token.op == op) {
            ++count;
        }
    }
    return count;
}

std::vector<std::string> SplitWords(const std::string& text) {
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

std::size_t LcsLength(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    std::vector<std::vector<std::size_t>> table(
        a.size() + 1, std::vector<std::size_t>(b.size() + 1, 0));
    for (std::size_t i = 1; i <= a.size(); ++i) {
        for (std::size_t j = 1; j <= b.size(); ++j) {
            if (a[i - 1] == b[j - 1]) {
                table[i][j] = table[i - 1][j - 1] + 1;
            } else {
                table[i][j] = std::max(table[i - 1][j], table[i][j - 1]);
            }
        }
    }
    return table[a.size()][b.size()];
}

}

TEST_CASE("empty and empty diffs to nothing") {
    const lwf::DiffResult result = lwf::Diff("", "");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    CHECK(result.tokens.empty());
}

TEST_CASE("identical text is all equal") {
    const lwf::DiffResult result = lwf::Diff("the cat sat", "the cat sat");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    REQUIRE(result.tokens.size() == 3);
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 3);
    CHECK(result.tokens[0].text == "the");
    CHECK(result.tokens[2].text == "sat");
}

TEST_CASE("insertion only counts inserted tokens") {
    const lwf::DiffResult result = lwf::Diff("the cat", "the cat sat");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 1);
    CHECK(result.deleted == 0);
    CHECK(HasInsert(result, "sat"));
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 2);
}

TEST_CASE("deletion only counts deleted tokens") {
    const lwf::DiffResult result = lwf::Diff("the cat sat", "the cat");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 1);
    CHECK(HasDelete(result, "sat"));
}

TEST_CASE("fully replaced text counts every token as replaced") {
    const lwf::DiffResult result = lwf::Diff("alpha beta gamma", "one two three");
    CHECK(result.replaced == 3);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 0);
}

TEST_CASE("single word swapped in place is one replacement") {
    const lwf::DiffResult result = lwf::Diff("the cat sat", "the dog sat");
    CHECK(result.replaced == 1);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    CHECK(HasDelete(result, "cat"));
    CHECK(HasInsert(result, "dog"));
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 2);
}

TEST_CASE("token order is preserved across the diff") {
    const lwf::DiffResult result = lwf::Diff("a b c d", "a x c y");
    REQUIRE(result.tokens.size() == 6);
    CHECK(result.tokens[0].op == lwf::DiffOp::Equal);
    CHECK(result.tokens[0].text == "a");
    CHECK(result.tokens[3].op == lwf::DiffOp::Equal);
    CHECK(result.tokens[3].text == "c");
}

TEST_CASE("mixed runs of whitespace tokenize to the same words") {
    const lwf::DiffResult result = lwf::Diff("a\tb\nc", "a b c");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 3);
}

TEST_CASE("non-breaking space separates tokens") {
    const lwf::DiffResult result = lwf::Diff("a\xC2\xA0" "b", "a b");
    CHECK(result.replaced == 0);
    CHECK(result.inserted == 0);
    CHECK(result.deleted == 0);
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 2);
}

TEST_CASE("whitespace-free text is a single token") {
    const lwf::DiffResult result = lwf::Diff("你好世界", "你好世界");
    CHECK(result.replaced == 0);
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 1);
}

TEST_CASE("equal count equals the LCS length for a battery of pairs") {
    const std::vector<std::pair<std::string, std::string>> pairs{
        {"a x", "x a"},
        {"a b c d", "a x c y"},
        {"one two three four", "two four"},
        {"alpha beta gamma delta epsilon", "beta delta alpha"},
        {"a a a b", "a b a a"},
        {"the cat sat on the mat", "the dog sat on the rug"},
        {"", "a b c"},
        {"a b c", ""},
        {"same words here", "same words here"},
    };
    for (const auto& pair : pairs) {
        const lwf::DiffResult result = lwf::Diff(pair.first, pair.second);
        CHECK(CountOp(result, lwf::DiffOp::Equal) ==
              LcsLength(SplitWords(pair.first), SplitWords(pair.second)));
        CHECK(CountOp(result, lwf::DiffOp::Equal) +
                  CountOp(result, lwf::DiffOp::Delete) ==
              SplitWords(pair.first).size());
        CHECK(CountOp(result, lwf::DiffOp::Equal) +
                  CountOp(result, lwf::DiffOp::Insert) ==
              SplitWords(pair.second).size());
    }
}

TEST_CASE("tie break emits delete before equal before insert for a swapped pair") {
    const lwf::DiffResult result = lwf::Diff("a x", "x a");
    REQUIRE(result.tokens.size() == 3);
    CHECK(result.tokens[0].op == lwf::DiffOp::Delete);
    CHECK(result.tokens[0].text == "a");
    CHECK(result.tokens[1].op == lwf::DiffOp::Equal);
    CHECK(result.tokens[1].text == "x");
    CHECK(result.tokens[2].op == lwf::DiffOp::Insert);
    CHECK(result.tokens[2].text == "a");
    CHECK(CountOp(result, lwf::DiffOp::Equal) == 1);
}
