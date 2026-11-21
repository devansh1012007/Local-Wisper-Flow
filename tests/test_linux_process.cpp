#include <doctest/doctest.h>

#include "os/LinuxProcess.h"

#include <chrono>
#include <filesystem>
#include <fstream>

using lwf::process::Command;
using lwf::process::RunResult;

namespace {

RunResult RunSh(const std::string& script, int timeout_ms = 5000) {
    Command command;
    command.executable = "sh";
    command.args = {"-c", script};
    return lwf::process::Run(command, "", timeout_ms);
}

}  // namespace

TEST_CASE("run captures merged stdout and exit code") {
    const RunResult result = RunSh("echo out; echo err 1>&2; exit 3");
    REQUIRE(result.launched);
    CHECK_FALSE(result.timed_out);
    CHECK(result.exit_code == 3);
    CHECK(result.output.find("out") != std::string::npos);
    CHECK(result.output.find("err") != std::string::npos);
}

TEST_CASE("run feeds stdin to the child") {
    Command command;
    command.executable = "cat";
    const RunResult result = lwf::process::Run(command, "hello stdin", 5000);
    REQUIRE(result.launched);
    CHECK(result.exit_code == 0);
    CHECK(result.output == "hello stdin");
}

TEST_CASE("run kills a stuck child on timeout") {
    const auto started = std::chrono::steady_clock::now();
    const RunResult result = RunSh("sleep 30", 200);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    CHECK(result.timed_out);
    CHECK(elapsed < 5000);
    CHECK_FALSE(result.launched);
}

TEST_CASE("run reports a missing executable without throwing") {
    Command command;
    command.executable = "definitely-not-a-real-binary-xyz";
    const RunResult result = lwf::process::Run(command, "", 1000);
    CHECK_FALSE(result.launched);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("run with an empty executable fails fast") {
    Command command;
    const RunResult result = lwf::process::Run(command, "", 100);
    CHECK_FALSE(result.launched);
}

TEST_CASE("large stdin payloads survive a slow consumer") {
    // >64 KB forces multiple pipe writes and exercises the non-blocking loop.
    Command command;
    command.executable = "wc";
    command.args = {"-c"};
    std::string big(200000, 'x');
    const RunResult result = lwf::process::Run(command, big, 5000);
    REQUIRE(result.launched);
    CHECK(result.output.find("200000") != std::string::npos);
}

TEST_CASE("exists finds PATH executables and rejects junk") {
    CHECK(lwf::process::Exists("sh"));
    CHECK(lwf::process::Exists("cat"));
    CHECK_FALSE(lwf::process::Exists("definitely-not-a-real-binary-xyz"));
    CHECK_FALSE(lwf::process::Exists(""));
    CHECK(lwf::process::Exists("/bin/sh"));
    CHECK_FALSE(lwf::process::Exists("/etc/hostname"));  // exists, not executable
}

TEST_CASE("argv is passed verbatim: no shell interpretation") {
    // A payload with shell metacharacters must arrive as one literal argv
    // element, proving transcripts can never become command syntax.
    Command command;
    command.executable = "printf";
    command.args = {"%s", "hello $(rm -rf /tmp) `id` ; ls"};
    const RunResult result = lwf::process::Run(command, "", 5000);
    REQUIRE(result.launched);
    CHECK(result.output == "hello $(rm -rf /tmp) `id` ; ls");
}
