#include "core/AppVersion.h"
#include "core/CrashReporter.h"
#include "core/Log.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

TEST_CASE("crash report contains version exception code fsm state and log tail") {
    const std::string base =
        (std::filesystem::temp_directory_path() / "lwf_crash_test").string();
    std::filesystem::remove_all(base);
    const std::string logs = base + "/logs";

    lwf::Log::Init(logs);
    lwf::Log::Info("line one before crash");
    lwf::Log::Warning("line two before crash");

    const std::string path = lwf::CrashReporter::WriteCrashReport(logs, "0xC0000005", "3",
                                                                  "access violation while injecting");
    REQUIRE_FALSE(path.empty());
    const std::string content = ReadFile(path);
    CHECK(content.find("Local Wisper Flow 0.1.0") != std::string::npos);
    CHECK(content.find("0xC0000005") != std::string::npos);
    CHECK(content.find("fsm_state: 3") != std::string::npos);
    CHECK(content.find("access violation while injecting") != std::string::npos);
    CHECK(content.find("line one before crash") != std::string::npos);
    CHECK(content.find("line two before crash") != std::string::npos);

    CHECK(lwf::CrashReporter::HasCrashReports(logs));
    const std::string found = lwf::CrashReporter::FindUnseenCrash(logs);
    CHECK(found == path);
    CHECK(lwf::CrashReporter::MarkSeen(found));
    CHECK_FALSE(lwf::CrashReporter::HasCrashReports(logs));
    std::filesystem::remove_all(base);
}

TEST_CASE("recent log lines returns the tail in order up to the cap") {
    const std::string base =
        (std::filesystem::temp_directory_path() / "lwf_ring_test").string();
    std::filesystem::remove_all(base);
    lwf::Log::Init(base + "/logs");
    for (int i = 0; i < 20; ++i) {
        lwf::Log::Info("ring line " + std::to_string(i));
    }
    const std::vector<std::string> lines = lwf::Log::RecentLines(5);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0].find("ring line 15") != std::string::npos);
    CHECK(lines[4].find("ring line 19") != std::string::npos);
    const std::vector<std::string> all = lwf::Log::RecentLines(1000);
    REQUIRE(all.size() >= 20);
    std::filesystem::remove_all(base);
}

TEST_CASE("app version constant is populated by the build") {
    const std::string version = lwf::AppVersion();
    CHECK_FALSE(version.empty());
    CHECK(version.find("0.0.0-dev") == std::string::npos);
}

}
