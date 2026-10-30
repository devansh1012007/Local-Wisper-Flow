#include <doctest/doctest.h>

#include "stt/cloud/GnaniResponse.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> Bytes(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

TEST_CASE("gnani response parses a success with transcript and header request id") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(ParseGnaniResponse(200, Bytes(R"({"transcript":"namaskara loka"})"), "req-123", success,
                             failure));
    CHECK(success.transcript == "namaskara loka");
    CHECK(success.request_id == "req-123");
}

TEST_CASE("gnani response treats an empty transcript as success") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(ParseGnaniResponse(200, Bytes(R"({"transcript":""})"), "req-456", success, failure));
    CHECK(success.transcript.empty());
    CHECK(success.request_id == "req-456");
}

TEST_CASE("gnani response falls back to alternate transcript field names") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(ParseGnaniResponse(200, Bytes(R"({"text":"via text"})"), "", success, failure));
    CHECK(success.transcript == "via text");
    CHECK(ParseGnaniResponse(200, Bytes(R"({"data":{"transcript":"nested"}})"), "", success,
                             failure));
    CHECK(success.transcript == "nested");
}

TEST_CASE("gnani response picks the body request id when the header is absent") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(ParseGnaniResponse(200, Bytes(R"({"transcript":"hi","request_id":"body-id"})"), "",
                             success, failure));
    CHECK(success.request_id == "body-id");
}

TEST_CASE("gnani response parses the error object shape") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(!ParseGnaniResponse(401,
                              Bytes(R"({"error":{"code":"invalid_api_key","message":"key rejected"}})"),
                              "req-1", success, failure));
    CHECK(failure.category == SttErrorCategory::Auth);
    CHECK(failure.http_status == 401);
    CHECK(failure.request_id == "req-1");
    CHECK(failure.detail == "invalid_api_key: key rejected");
}

TEST_CASE("gnani response parses the error string shape") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(!ParseGnaniResponse(429, Bytes(R"({"error":"too many requests"})"), "", success,
                              failure));
    CHECK(failure.category == SttErrorCategory::RateLimited);
    CHECK(failure.detail == "too many requests");
}

TEST_CASE("gnani response parses the message and detail shapes") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(!ParseGnaniResponse(500, Bytes(R"({"message":"upstream burst"})"), "req-2", success,
                              failure));
    CHECK(failure.category == SttErrorCategory::Server);
    CHECK(failure.detail == "upstream burst");
    CHECK(!ParseGnaniResponse(400, Bytes(R"({"detail":"audio must be 16 kHz"})"), "", success,
                              failure));
    CHECK(failure.category == SttErrorCategory::InvalidRequest);
    CHECK(failure.detail == "audio must be 16 kHz");
}

TEST_CASE("gnani response classifies transport level status codes") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(!ParseGnaniResponse(403, Bytes(R"({})"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::Auth);
    CHECK(!ParseGnaniResponse(408, Bytes(R"({})"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::Timeout);
    CHECK(!ParseGnaniResponse(413, Bytes(R"({})"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::PayloadTooLarge);
    CHECK(!ParseGnaniResponse(503, Bytes("{}"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::Server);
}

TEST_CASE("gnani response survives malformed bodies without throwing") {
    GnaniSuccess success;
    GnaniFailure failure;
    CHECK(!ParseGnaniResponse(200, Bytes("not json at all"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::Unknown);
    CHECK(!ParseGnaniResponse(500, Bytes("still not json"), "", success, failure));
    CHECK(failure.category == SttErrorCategory::Server);
    CHECK(!ParseGnaniResponse(200, Bytes("[]"), "", success, failure));
    CHECK(failure.detail == "success body is not a JSON object");
    CHECK(!ParseGnaniResponse(200, Bytes(R"({"unexpected":"shape"})"), "", success, failure));
    CHECK(failure.detail == "no transcript field in success body");
}
