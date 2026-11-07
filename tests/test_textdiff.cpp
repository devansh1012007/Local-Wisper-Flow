#include "core/TextDiff.h"

#include <doctest/doctest.h>
#include <string>

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
