#include <doctest/doctest.h>

#include "core/InjectionException.h"
#include "os/LinuxNotifier.h"
#include "os/LinuxTextInjector.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using lwf::process::Command;
using lwf::process::RunResult;

namespace {

// Scoped environment override helper.
class EnvOverride {
public:
    EnvOverride(const std::string& name, const std::string& value) : name_(name) {
        const char* old = std::getenv(name.c_str());
        if (old != nullptr) {
            previous_ = old;
            had_previous_ = true;
        }
        ::setenv(name.c_str(), value.c_str(), 1);
    }
    ~EnvOverride() {
        if (had_previous_) {
            ::setenv(name_.c_str(), previous_.c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    std::string previous_;
    bool had_previous_ = false;
};

// A PATH pointing at a temp dir where tests plant fake tools.
class FakePath {
public:
    FakePath() : dir_(std::filesystem::temp_directory_path() / "lwf-fake-bin") {
        std::filesystem::create_directories(dir_);
        old_path_ = std::getenv("PATH") != nullptr ? std::getenv("PATH") : "";
        ::setenv("PATH", dir_.c_str(), 1);
    }
    ~FakePath() {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
        ::setenv("PATH", old_path_.c_str(), 1);
    }

    void Plant(const std::string& name) {
        const std::filesystem::path script = dir_ / name;
        std::ofstream(script) << "#!/bin/sh\nexit 0\n";
        std::filesystem::permissions(script,
                                     std::filesystem::perms::owner_exec |
                                         std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec |
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::group_read |
                                         std::filesystem::perms::others_read);
    }

private:
    std::filesystem::path dir_;
    std::string old_path_;
};

}  // namespace

TEST_CASE("session detection honors the override and the desktop variables") {
    {
        EnvOverride mode("LWF_INJECT_MODE", "x11");
        EnvOverride wayland("WAYLAND_DISPLAY", "");
        ::unsetenv("WAYLAND_DISPLAY");
        CHECK(LinuxTextInjector::DetectSession() == LinuxTextInjector::Session::X11);
    }
    {
        EnvOverride mode("LWF_INJECT_MODE", "wayland");
        CHECK(LinuxTextInjector::DetectSession() == LinuxTextInjector::Session::Wayland);
    }
    {
        EnvOverride mode("LWF_INJECT_MODE", "invalid");
        EnvOverride wayland("WAYLAND_DISPLAY", "wayland-0");
        EnvOverride x11("DISPLAY", ":0");
        CHECK(LinuxTextInjector::DetectSession() == LinuxTextInjector::Session::Wayland);
    }
    {
        EnvOverride mode("LWF_INJECT_MODE", "");
        ::unsetenv("LWF_INJECT_MODE");
        EnvOverride wayland("WAYLAND_DISPLAY", "");
        ::unsetenv("WAYLAND_DISPLAY");
        EnvOverride x11("DISPLAY", ":0");
        CHECK(LinuxTextInjector::DetectSession() == LinuxTextInjector::Session::X11);
    }
    {
        EnvOverride mode("LWF_INJECT_MODE", "");
        ::unsetenv("LWF_INJECT_MODE");
        EnvOverride wayland("WAYLAND_DISPLAY", "");
        ::unsetenv("WAYLAND_DISPLAY");
        EnvOverride x11("DISPLAY", "");
        ::unsetenv("DISPLAY");
        CHECK(LinuxTextInjector::DetectSession() == LinuxTextInjector::Session::Headless);
    }
}

TEST_CASE("char to keysym maps control characters and unicode") {
    CHECK(LinuxTextInjector::CharToKeysym(U'\n') == 0xff0d);
    CHECK(LinuxTextInjector::CharToKeysym(U'\r') == 0xff0d);
    CHECK(LinuxTextInjector::CharToKeysym(U'\t') == 0xff09);
    CHECK(LinuxTextInjector::CharToKeysym(U'a') == 0x61);
    CHECK(LinuxTextInjector::CharToKeysym(U'Z') == 0x5A);
    CHECK(LinuxTextInjector::CharToKeysym(U' ') == 0x20);
    CHECK(LinuxTextInjector::CharToKeysym(U'\x7F') == 0);  // DEL: untypable
    CHECK(LinuxTextInjector::CharToKeysym(U'\x01') == 0);  // control: untypable
    CHECK(LinuxTextInjector::CharToKeysym(U'\u00e9') == 0xe9);          // Latin-1
    CHECK(LinuxTextInjector::CharToKeysym(U'\u20ac') == 0x010020ac);    // euro sign
    CHECK(LinuxTextInjector::CharToKeysym(U'\U0001F600') == 0x0101F600); // emoji
}

TEST_CASE("clipboard tool choice follows session preference order") {
    FakePath path;
    {
        // Nothing planted: no tool.
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::Wayland)
                  .empty());
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::X11).empty());
    }
    path.Plant("xclip");
    {
        // Wayland prefers wl-copy but falls back to xclip; X11 picks xclip.
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::Wayland) ==
              "xclip");
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::X11) ==
              "xclip");
    }
    path.Plant("wl-copy");
    {
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::Wayland) ==
              "wl-copy");
        CHECK(LinuxTextInjector::ChooseClipboardTool(LinuxTextInjector::Session::X11) ==
              "xclip");
    }
}

TEST_CASE("inject throws on headless and falls back to clipboard on wayland") {
    LinuxTextInjector injector;

    // Record every command the fake runner sees, with its stdin payload.
    struct Call {
        Command command;
        std::string stdin_data;
    };
    std::vector<Call> calls;
    injector.SetRunner([&calls](const Command& command, const std::string& stdin_data,
                                int) {
        calls.push_back(Call{command, stdin_data});
        RunResult result;
        result.launched = true;
        result.exit_code = 0;
        return result;
    });

    {
        EnvOverride mode("LWF_INJECT_MODE", "");
        ::unsetenv("LWF_INJECT_MODE");
        EnvOverride wayland("WAYLAND_DISPLAY", "");
        ::unsetenv("WAYLAND_DISPLAY");
        EnvOverride x11("DISPLAY", "");
        ::unsetenv("DISPLAY");
        CHECK_THROWS_AS(injector.InjectText("hi"), InjectionException);
        CHECK(calls.empty());  // headless fails before any subprocess runs
    }
    {
        EnvOverride mode("LWF_INJECT_MODE", "wayland");
        FakePath path;
        path.Plant("wl-copy");  // chosen tool; the fake runner intercepts exec
        injector.InjectText("hello wayland");
        // wl-paste is not on PATH, so the readback is skipped: exactly one
        // copy call carrying the payload through stdin, never through argv.
        REQUIRE(calls.size() == 1);
        CHECK(calls[0].stdin_data == "hello wayland");
        CHECK(calls[0].command.executable == "wl-copy");
        bool payload_in_argv = false;
        for (const std::string& arg : calls[0].command.args) {
            if (arg == "hello wayland") {
                payload_in_argv = true;
            }
        }
        CHECK_FALSE(payload_in_argv);
    }
}

TEST_CASE("notifier builds the notify-send argv without shell characters") {
    std::vector<Command> calls;
    LinuxNotifier notifier([&calls](const Command& command, const std::string&,
                                    int) -> RunResult {
        calls.push_back(command);
        RunResult result;
        result.launched = true;
        result.exit_code = 0;
        return result;
    });
    // Force "available" by planting a fake notify-send on a controlled PATH.
    FakePath path;
    path.Plant("notify-send");
    notifier.Notify("title with; rm -rf", "body $(whoami)", 3000,
                    LinuxNotifier::Urgency::Critical);
    REQUIRE(calls.size() == 1);
    bool saw_title = false;
    bool saw_body = false;
    bool saw_critical = false;
    for (const std::string& arg : calls[0].args) {
        if (arg == "title with; rm -rf") {
            saw_title = true;
        }
        if (arg == "body $(whoami)") {
            saw_body = true;
        }
        if (arg == "critical") {
            saw_critical = true;
        }
    }
    CHECK(saw_title);
    CHECK(saw_body);
    CHECK(saw_critical);
    CHECK(calls[0].executable == "notify-send");
}

TEST_CASE("disabled notifier skips subprocesses entirely") {
    std::vector<Command> calls;
    LinuxNotifier notifier([&calls](const Command& command, const std::string&, int) -> RunResult {
        calls.push_back(command);
        RunResult result;
        result.launched = true;
        result.exit_code = 0;
        return result;
    });
    notifier.SetEnabled(false);
    CHECK_FALSE(notifier.Enabled());
    CHECK(notifier.Notify("t", "b", 100));  // silenced is success, not error
    CHECK(calls.empty());
    notifier.SetEnabled(true);
    notifier.SetEnabled(false);
    notifier.NotifyError(ErrorCode::ModelMissing, "detail");
    CHECK(calls.empty());
}
