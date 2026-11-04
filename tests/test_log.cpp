#include "core/ErrorCode.h"
#include "core/Log.h"

#include <doctest/doctest.h>

using lwf::Log;
using lwf::LogLevel;

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::path("/tmp") / (std::string("lwf_log_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string ReadAll(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

}

TEST_CASE("log writes, rotates and honors level") {
    const std::string dir = TempDir("rotate");
    Log::Init(dir);
    Log::SetRotationLimits(200, 3);
    Log::SetLevel(LogLevel::Debug);
    for (int i = 0; i < 30; ++i) {
        Log::Info("filler line number " + std::to_string(i));
    }

    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++files;
    }
    CHECK(files <= 4);
    CHECK(std::filesystem::exists(dir + "/lwf.log"));
    const std::string active = ReadAll(dir + "/lwf.log");
    CHECK(active.size() <= 400);

    Log::SetLevel(LogLevel::Error);
    Log::Info("this info must not appear");
    CHECK(ReadAll(dir + "/lwf.log").find("must not appear") == std::string::npos);
    Log::Error("this error must appear");
    CHECK(ReadAll(dir + "/lwf.log").find("must appear") != std::string::npos);
}

TEST_CASE("transcript text never lands in the log unless opted in") {
    const std::string dir = TempDir("privacy");
    Log::Init(dir);
    Log::SetLevel(LogLevel::Debug);
    Log::SetTranscriptLogging(false);
    Log::Transcript("super secret dictated sentence");
    Log::Info("normal operation line");
    CHECK(ReadAll(dir + "/lwf.log").find("super secret") == std::string::npos);

    Log::SetTranscriptLogging(true);
    Log::Transcript("opted in sentence");
    CHECK(ReadAll(dir + "/lwf.log").find("opted in sentence") != std::string::npos);
    CHECK_FALSE(Log::TranscriptLogging() == false);
}

TEST_CASE("every error code has a name and a user message") {
    const ErrorCode codes[] = {
        ErrorCode::MicUnavailable,      ErrorCode::MicLost,
        ErrorCode::ModelMissing,        ErrorCode::ModelLoadFailed,
        ErrorCode::SttFailed,           ErrorCode::LlmFailed,
        ErrorCode::LlmTimeout,          ErrorCode::InjectFailed,
        ErrorCode::InjectBlockedElevated, ErrorCode::ClipboardBusy,
        ErrorCode::HookInstallFailed,   ErrorCode::ConfigCorrupt,
        ErrorCode::DownloadFailed,      ErrorCode::ChecksumMismatch,
        ErrorCode::Unknown,
    };
    for (const ErrorCode code : codes) {
        CHECK(std::string(ToString(code)).size() > 3);
        CHECK(std::string(UserMessage(code)).size() > 3);
    }
    CHECK(std::string(ToString(ErrorCode::Unknown)) == "Unknown");
}
