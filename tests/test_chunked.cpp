#include "Fakes.h"

#include "core/InjectorDecorators.h"

#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {

using namespace lwf_test;

}

TEST_CASE("short text goes through as one chunk") {
    RecordingInjector inner;
    ChunkedInjector chunked(inner);
    chunked.InjectText("hello world");
    REQUIRE(inner.injected.size() == 1);
    CHECK(inner.injected[0] == "hello world");
}

TEST_CASE("long text splits at word boundaries") {
    RecordingInjector inner;
    ChunkedInjector chunked(inner, 32);
    std::string text;
    for (int i = 0; i < 20; ++i) {
        text += "word" + std::to_string(i) + " ";
    }
    chunked.InjectText(text);

    REQUIRE(inner.injected.size() > 1);
    std::string joined;
    for (const std::string& part : inner.injected) {
        joined += part;
        CHECK(part.size() <= 33);
    }
    CHECK(joined == text);
}

TEST_CASE("no word is cut in half across chunks") {
    RecordingInjector inner;
    ChunkedInjector chunked(inner, 16);
    const std::string text = "alpha beta gamma delta epsilon zeta eta theta iota kappa";
    chunked.InjectText(text);

    std::string joined;
    for (const std::string& part : inner.injected) {
        joined += part;
        if (part.size() > 1 && part.front() != ' ') {
            CHECK(part.front() != 'a' || part == "alpha ");
        }
    }
    CHECK(joined == text);
}

TEST_CASE("very long unbreakable run is cut hard") {
    RecordingInjector inner;
    ChunkedInjector chunked(inner, 8);
    chunked.InjectText(std::string(50, 'x'));
    REQUIRE(inner.injected.size() == 7);
    CHECK(inner.injected.front().size() == 8);
}

TEST_CASE("newlines act as break points") {
    RecordingInjector inner;
    ChunkedInjector chunked(inner, 12);
    const std::string text = "first line\nsecond line\nthird line";
    chunked.InjectText(text);
    std::string joined;
    for (const std::string& part : inner.injected) {
        joined += part;
    }
    CHECK(joined == text);
    CHECK(inner.injected.size() == 3);
}
