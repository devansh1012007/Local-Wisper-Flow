#include "core/Onboarding.h"
#include "core/Sha256.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << content;
}

}

TEST_CASE("sha256 known vectors") {
    CHECK(lwf_sha256::HexDigest("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(lwf_sha256::HexDigest("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(lwf_sha256::HexDigest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(lwf_sha256::HexDigest(std::string(64, 'a')) ==
          "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

TEST_CASE("sha256 of a file matches the digest of its content") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_sha.bin").string();
    WriteFile(path, "hello onboarding file");
    bool ok = false;
    const std::string digest = lwf_sha256::HexDigestOfFile(path, ok);
    CHECK(ok);
    CHECK(digest == lwf_sha256::HexDigest("hello onboarding file"));

    bool missing_ok = true;
    CHECK(lwf_sha256::HexDigestOfFile(
              (std::filesystem::temp_directory_path() / "lwf_sha_missing.bin").string(),
              missing_ok)
              .empty());
    CHECK_FALSE(missing_ok);
}

TEST_CASE("models catalog parses entries and flags missing hashes") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_models.json").string();
    WriteFile(path, R"json({
        "whisper": [
            {"name": "ggml-base.en.bin", "url": "https://example/ggml-base.en.bin",
             "size_bytes": 148000000, "sha256": "abc123"}
        ],
        "llm": [
            {"name": "qwen2.5-1.5b-instruct-q4_k_m.gguf", "url": "https://example/qwen.gguf",
             "size_bytes": 986000000}
        ]
    })json");

    const Onboarding::ModelCatalog catalog = Onboarding::ParseModelsJson(path);
    REQUIRE(catalog.whisper.size() == 1);
    REQUIRE(catalog.llm.size() == 1);
    CHECK(catalog.whisper[0].name == "ggml-base.en.bin");
    CHECK(catalog.whisper[0].sha256 == "abc123");
    CHECK(catalog.llm[0].size_bytes == 986000000);
    REQUIRE(catalog.warnings.size() == 1);
    CHECK(catalog.warnings[0].find("qwen") != std::string::npos);

    WriteFile(path, "{ broken");
    const Onboarding::ModelCatalog broken = Onboarding::ParseModelsJson(path);
    CHECK(broken.whisper.empty());
    CHECK_FALSE(broken.warnings.empty());

    const Onboarding::ModelCatalog missing = Onboarding::ParseModelsJson(
        (std::filesystem::temp_directory_path() / "lwf_models_absent.json").string());
    CHECK(missing.whisper.empty());
    CHECK_FALSE(missing.warnings.empty());
}

TEST_CASE("resume planner covers every branch") {
    const Onboarding::ResumePlan fresh = Onboarding::PlanResume(0, 1000, "sha", "");
    CHECK(fresh.valid);
    CHECK(fresh.start_offset == 0);
    CHECK(fresh.verify_required);

    const Onboarding::ResumePlan partial = Onboarding::PlanResume(400, 1000, "sha", "");
    CHECK(partial.valid);
    CHECK(partial.start_offset == 400);
    CHECK(partial.verify_required);

    const Onboarding::ResumePlan no_verify = Onboarding::PlanResume(400, 1000, "", "");
    CHECK(no_verify.valid);
    CHECK_FALSE(no_verify.verify_required);

    const Onboarding::ResumePlan complete = Onboarding::PlanResume(1000, 1000, "sha", "sha");
    CHECK(complete.valid);
    CHECK(complete.start_offset == 0);
    CHECK_FALSE(complete.verify_required);

    const Onboarding::ResumePlan bad_hash = Onboarding::PlanResume(1000, 1000, "sha", "other");
    CHECK_FALSE(bad_hash.valid);
    CHECK(bad_hash.error.find("checksum") != std::string::npos);

    const Onboarding::ResumePlan oversized = Onboarding::PlanResume(2000, 1000, "sha", "");
    CHECK_FALSE(oversized.valid);

    const Onboarding::ResumePlan unknown_remote = Onboarding::PlanResume(400, 0, "sha", "");
    CHECK_FALSE(unknown_remote.valid);
}
