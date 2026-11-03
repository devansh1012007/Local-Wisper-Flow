#include <doctest/doctest.h>

#include "core/Redact.h"

#include <string>

TEST_CASE("redact secret replaces every occurrence") {
    const std::string key = "sk-secret-123";
    const std::string text = "Authorization: " + key + " and echo " + key + " end";
    const std::string redacted = lwf::RedactSecret(text, key);
    CHECK(redacted.find("[redacted]") != std::string::npos);
    CHECK(redacted.find(key) == std::string::npos);
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = redacted.find("[redacted]", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    CHECK(count == 2);
}

TEST_CASE("redact secret keeps text without the secret intact") {
    const std::string text = "nothing sensitive here";
    CHECK(lwf::RedactSecret(text, "sk-absent") == text);
}

TEST_CASE("redact secret handles empty inputs and full replacement") {
    const std::string text = "value";
    CHECK(lwf::RedactSecret(text, std::string()) == text);
    CHECK(lwf::RedactSecret(std::string(), "key").empty());
    CHECK(lwf::RedactSecret(text, text) == "[redacted]");
}

TEST_CASE("redact auth values masks sensitive header lines case-insensitively") {
    const std::string request = "POST /v1/stt HTTP/1.1\r\n"
                                "Host: api.vachana.ai\r\n"
                                "X-API-Key-ID: key-id-1\r\n"
                                "Content-Type: multipart/form-data\r\n";
    const std::string redacted = lwf::RedactAuthValues(request);
    CHECK(redacted.find("key-id-1") == std::string::npos);
    CHECK(redacted.find("X-API-Key-ID: [redacted]") != std::string::npos);
    CHECK(redacted.find("Host: api.vachana.ai") != std::string::npos);
    CHECK(redacted.find("Content-Type: multipart/form-data") != std::string::npos);
}

TEST_CASE("redact auth values masks bearer tokens and cookies") {
    const std::string request = "authorization: Bearer muse-key-99\n"
                                "XI-API-KEY: eleven-key-7\r\n"
                                "Cookie: session=abc\n"
                                "Accept: application/json\n";
    const std::string redacted = lwf::RedactAuthValues(request);
    CHECK(redacted.find("muse-key-99") == std::string::npos);
    CHECK(redacted.find("eleven-key-7") == std::string::npos);
    CHECK(redacted.find("session=abc") == std::string::npos);
    CHECK(redacted.find("Accept: application/json") != std::string::npos);
    CHECK(redacted.find("authorization: [redacted]") != std::string::npos);
    CHECK(redacted.find("XI-API-KEY: [redacted]") != std::string::npos);
}

TEST_CASE("redact auth values leaves lines without a colon untouched") {
    const std::string text = "plain line\nno colon here\nX-API-Key-Id: v\n";
    const std::string redacted = lwf::RedactAuthValues(text);
    CHECK(redacted.find("plain line") != std::string::npos);
    CHECK(redacted.find("no colon here") != std::string::npos);
    CHECK(redacted.find("v") == std::string::npos);
}
