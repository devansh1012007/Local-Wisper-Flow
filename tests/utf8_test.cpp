#include <doctest/doctest.h>

#include "core/Utf8.h"

#include <string>

TEST_CASE("trailing partial utf8 is detected and trimmed") {
    CHECK(Utf8::TrimTrailingPartial("hello") == "hello");
    CHECK(Utf8::TrimTrailingPartial("h\xc3") == "h");
    CHECK(Utf8::TrimTrailingPartial("hi\xe0\xa4") == "hi");
    CHECK(Utf8::TrimTrailingPartial("hey\xf0\x9f\x98") == "hey");
    CHECK(Utf8::TrimTrailingPartial("\xf0\x9f\x98\x80") == "\xf0\x9f\x98\x80");
    CHECK(Utf8::TrimTrailingPartial("caf\xc3\xa9") == "caf\xc3\xa9");
    CHECK(Utf8::TrimTrailingPartial("\xe0\xa4\x95\xe0\xa4\xbf") == "\xe0\xa4\x95\xe0\xa4\xbf");
}

TEST_CASE("trailing partial length handles degenerate inputs") {
    CHECK(Utf8::TrailingPartialLength("") == 0);
    CHECK(Utf8::TrailingPartialLength("\x80\x80\x80\x80") == 0);
    CHECK(Utf8::TrailingPartialLength("a\x80") == 0);
    CHECK(Utf8::TrailingPartialLength("a\xc3") == 1);
    CHECK(Utf8::TrailingPartialLength("a\xe0\xa4") == 2);
    CHECK(Utf8::TrailingPartialLength("a\xf0\x9f\x98") == 3);
}

TEST_CASE("normalize newlines collapses crlf and lone cr") {
    CHECK(Utf8::NormalizeNewlines("a\r\nb") == "a\nb");
    CHECK(Utf8::NormalizeNewlines("a\rb") == "a\nb");
    CHECK(Utf8::NormalizeNewlines("a\nb") == "a\nb");
    CHECK(Utf8::NormalizeNewlines("a\r\r\nb") == "a\n\nb");
    CHECK(Utf8::NormalizeNewlines("") == "");
}

TEST_CASE("newlines to crlf never doubles existing crlf") {
    CHECK(Utf8::NewlinesToCrlf("a\nb") == "a\r\nb");
    CHECK(Utf8::NewlinesToCrlf("a\r\nb") == "a\r\nb");
    CHECK(Utf8::NewlinesToCrlf("a\rb") == "a\r\nb");
    CHECK(Utf8::NewlinesToCrlf("a\r\r\nb") == "a\r\n\r\nb");
    CHECK(Utf8::NewlinesToCrlf("line1\nline2\n") == "line1\r\nline2\r\n");
}
