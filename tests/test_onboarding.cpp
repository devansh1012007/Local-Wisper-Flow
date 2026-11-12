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

TEST_CASE("download model lifecycle: start, monotonic progress, terminal states") {
    Onboarding::DownloadModel model;
    const Onboarding::DownloadSnapshot fresh = model.Snapshot();
    CHECK(fresh.state == Onboarding::DownloadState::Idle);
    CHECK(fresh.downloaded == 0);
    CHECK(fresh.total == 0);
    CHECK(fresh.destination.empty());
    CHECK_FALSE(model.IsDownloading());

    // An empty destination can never start.
    CHECK_FALSE(model.Start("", 1000));
    CHECK(model.Snapshot().state == Onboarding::DownloadState::Idle);

    CHECK(model.Start("models/x.bin", 1000));
    CHECK(model.IsDownloading());
    CHECK(model.Snapshot().state == Onboarding::DownloadState::Downloading);
    CHECK(model.Snapshot().destination == "models/x.bin");
    CHECK(model.Snapshot().total == 1000);
    CHECK(model.Snapshot().downloaded == 0);

    // Start-while-downloading is refused and changes nothing.
    CHECK_FALSE(model.Start("models/y.bin", 5));
    CHECK(model.Snapshot().destination == "models/x.bin");
    CHECK(model.Snapshot().total == 1000);

    // Progress is monotonic and clamps to the total.
    model.OnProgress(400);
    CHECK(model.Snapshot().downloaded == 400);
    model.OnProgress(300);
    CHECK(model.Snapshot().downloaded == 400);
    model.OnProgress(5000);
    CHECK(model.Snapshot().downloaded == 1000);
    CHECK(model.ProgressFraction() == 1.0);

    // Terminal transitions apply only from Downloading, and success keeps
    // the destination info for the caller to persist.
    model.OnSuccess();
    CHECK(model.Snapshot().state == Onboarding::DownloadState::Success);
    CHECK_FALSE(model.IsDownloading());
    CHECK(model.Snapshot().destination == "models/x.bin");
    CHECK(model.Snapshot().downloaded == 1000);
    // Late progress and a second terminal transition are ignored.
    model.OnProgress(0);
    model.OnCancelled();
    CHECK(model.Snapshot().state == Onboarding::DownloadState::Success);
    CHECK(model.Snapshot().downloaded == 1000);
}

TEST_CASE("download model: indeterminate total and cancel/failure paths") {
    Onboarding::DownloadModel indeterminate;
    CHECK(indeterminate.Start("models/y.bin", 0));
    CHECK(indeterminate.ProgressFraction() == -1.0);
    indeterminate.OnProgress(1234);
    CHECK(indeterminate.Snapshot().downloaded == 1234);
    CHECK(indeterminate.ProgressFraction() == -1.0);
    indeterminate.OnCancelled();
    CHECK(indeterminate.Snapshot().state == Onboarding::DownloadState::Cancelled);

    Onboarding::DownloadModel failed;
    CHECK(failed.Start("models/z.bin", 10));
    Onboarding::DownloadError error;
    error.code = ErrorCode::ChecksumMismatch;
    error.detail = "downloaded file failed verification";
    failed.OnFailed(error);
    const Onboarding::DownloadSnapshot snapshot = failed.Snapshot();
    CHECK(snapshot.state == Onboarding::DownloadState::Failed);
    CHECK(snapshot.error.code == ErrorCode::ChecksumMismatch);
    CHECK(snapshot.error.detail == "downloaded file failed verification");

    // Terminal calls from Idle are no-ops.
    Onboarding::DownloadModel idle;
    idle.OnSuccess();
    idle.OnCancelled();
    idle.OnFailed(error);
    idle.OnProgress(9);
    CHECK(idle.Snapshot().state == Onboarding::DownloadState::Idle);
    CHECK(idle.Snapshot().downloaded == 0);

    // Reset returns any state to Idle and clears everything.
    failed.Reset();
    CHECK(failed.Snapshot().state == Onboarding::DownloadState::Idle);
    CHECK(failed.Snapshot().downloaded == 0);
    CHECK(failed.Snapshot().destination.empty());
    CHECK(failed.Snapshot().error.detail.empty());
    failed.Reset();
    CHECK(failed.Start("models/again.bin", 1));
    CHECK(failed.Snapshot().state == Onboarding::DownloadState::Downloading);
    CHECK(failed.Snapshot().total == 1);
}

TEST_CASE("download error mapping preserves codes and labels other codes unknown") {
    const Onboarding::DownloadError failed =
        Onboarding::MapDownloadError(ErrorCode::DownloadFailed, "request failed");
    CHECK(failed.code == ErrorCode::DownloadFailed);
    CHECK(failed.detail == "request failed");

    const Onboarding::DownloadError bad =
        Onboarding::MapDownloadError(ErrorCode::ChecksumMismatch, "mismatch");
    CHECK(bad.code == ErrorCode::ChecksumMismatch);
    CHECK(bad.detail == "mismatch");

    const Onboarding::DownloadError other = Onboarding::MapDownloadError(ErrorCode::SttFailed, "x");
    CHECK(other.code == ErrorCode::Unknown);
    CHECK(other.detail == "x");

    // The transfer layer throws the cancel detail when its progress
    // callback returns false; the predicate pins that string contract.
    CHECK(Onboarding::IsCancellationDetail("download cancelled by user"));
    CHECK_FALSE(Onboarding::IsCancellationDetail("request failed"));
    CHECK_FALSE(Onboarding::IsCancellationDetail(""));
}

TEST_CASE("download start plan skips only a verified-complete destination") {
    // Verified complete: skip, no transfer.
    const Onboarding::DownloadStartPlan done =
        Onboarding::PlanDownloadStart(1000, 1000, "sha", "sha");
    CHECK(done.skip);

    // Hash mismatch on a complete file: download again from scratch.
    const Onboarding::DownloadStartPlan bad =
        Onboarding::PlanDownloadStart(1000, 1000, "sha", "other");
    CHECK_FALSE(bad.skip);
    CHECK(bad.resume_offset == 0);

    // Unknown remote size: never skip.
    const Onboarding::DownloadStartPlan unknown =
        Onboarding::PlanDownloadStart(0, 0, "", "");
    CHECK_FALSE(unknown.skip);

    // Partial file: resume offset carries through, no skip.
    const Onboarding::DownloadStartPlan partial =
        Onboarding::PlanDownloadStart(400, 1000, "sha", "");
    CHECK_FALSE(partial.skip);
    CHECK(partial.resume_offset == 400);

    // No expected hash and no file: full download, no skip.
    const Onboarding::DownloadStartPlan fresh =
        Onboarding::PlanDownloadStart(0, 1000, "", "");
    CHECK_FALSE(fresh.skip);
    CHECK(fresh.resume_offset == 0);
}

TEST_CASE("download file name safety rejects separators and parents") {
    CHECK(Onboarding::IsSafeFileName("ggml-base.en.bin"));
    CHECK(Onboarding::IsSafeFileName("qwen2.5-1.5b-instruct-q4_k_m.gguf"));
    CHECK_FALSE(Onboarding::IsSafeFileName(""));
    CHECK_FALSE(Onboarding::IsSafeFileName("../evil.bin"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a/b.bin"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a\\b.bin"));
    CHECK_FALSE(Onboarding::IsSafeFileName(".."));
    CHECK_FALSE(Onboarding::IsSafeFileName("."));
    // MAJOR-2 hardening: drive/ADS, Windows-invalid chars, trailing dot/space.
    CHECK_FALSE(Onboarding::IsSafeFileName("C:evil.bin"));
    CHECK_FALSE(Onboarding::IsSafeFileName("foo:bar"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a<>b"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a|b"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a?b"));
    CHECK_FALSE(Onboarding::IsSafeFileName("a*b"));
    CHECK_FALSE(Onboarding::IsSafeFileName("trailing."));
    CHECK_FALSE(Onboarding::IsSafeFileName("trailing "));
}

TEST_CASE("models catalog parses optional display metadata with backward-compatible defaults") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_models_meta.json").string();

    // Old shape (no display metadata): every new field defaults to empty.
    WriteFile(path, R"json({
        "whisper": [
            {"name": "ggml-base.en.bin", "url": "https://example/ggml-base.en.bin",
             "sha256": "abc123", "size_bytes": 148000000}
        ],
        "llm": []
    })json");
    const Onboarding::ModelCatalog legacy = Onboarding::ParseModelsJson(path);
    REQUIRE(legacy.whisper.size() == 1);
    CHECK(legacy.whisper[0].display_name.empty());
    CHECK(legacy.whisper[0].language.empty());
    CHECK(legacy.whisper[0].license.empty());
    CHECK(legacy.whisper[0].notes.empty());
    CHECK(Onboarding::ModelDisplayName(legacy.whisper[0]) == "ggml-base.en.bin");

    // New shape: metadata parsed, wrong-typed fields tolerated as empty.
    WriteFile(path, R"json({
        "whisper": [
            {"name": "ggml-large-v3-turbo.bin", "url": "https://example/t.bin",
             "sha256": "abc123", "size_bytes": 1624555275,
             "display_name": "Whisper Large v3 Turbo",
             "language": "hi-hinglish", "license": "MIT",
             "notes": "Best local Hinglish accuracy",
             "size_bytes_wrong_type": 7}
        ],
        "llm": [
            {"name": "q.gguf", "url": "https://example/q.gguf", "sha256": "def456",
             "display_name": 42, "language": ["en"]}
        ]
    })json");
    const Onboarding::ModelCatalog modern = Onboarding::ParseModelsJson(path);
    REQUIRE(modern.whisper.size() == 1);
    CHECK(modern.whisper[0].display_name == "Whisper Large v3 Turbo");
    CHECK(modern.whisper[0].language == "hi-hinglish");
    CHECK(modern.whisper[0].license == "MIT");
    CHECK(modern.whisper[0].notes == "Best local Hinglish accuracy");
    CHECK(Onboarding::ModelDisplayName(modern.whisper[0]) == "Whisper Large v3 Turbo");
    REQUIRE(modern.llm.size() == 1);
    CHECK(modern.llm[0].display_name.empty());
    CHECK(modern.llm[0].language.empty());
}

TEST_CASE("model display helpers map languages and sizes") {
    Onboarding::ModelDownload model;
    model.name = "a.bin";
    CHECK(Onboarding::ModelDisplayName(model) == "a.bin");
    model.display_name = "Whisper Base (English)";
    CHECK(Onboarding::ModelDisplayName(model) == "Whisper Base (English)");

    CHECK(Onboarding::LanguageDisplayName("en") == "English");
    CHECK(Onboarding::LanguageDisplayName("hi-hinglish") == "Hinglish (Hindi + English)");
    CHECK(Onboarding::LanguageDisplayName("multi") == "Multilingual");
    CHECK(Onboarding::LanguageDisplayName("tlh") == "Other");
    CHECK(Onboarding::LanguageDisplayName("") == "Other");

    CHECK(Onboarding::ModelSizeLabel(0) == "unknown size");
    CHECK(Onboarding::ModelSizeLabel(147964211) == "141 MB");
    CHECK(Onboarding::ModelSizeLabel(1624555275) == "1.5 GB");
    CHECK(Onboarding::ModelSizeLabel(1024ull * 1024ull * 1024ull) == "1.0 GB");
}
