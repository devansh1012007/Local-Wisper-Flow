#include "Fakes.h"

#include "core/AppContextResolver.h"
#include "core/DictationCleanup.h"
#include "core/TextPipeline.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

}

TEST_CASE("built-in profiles resolve by precedence") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    CHECK(resolver.Loaded());

    CHECK(resolver.Resolve("Slack.exe", "").name == "chat");
    CHECK(resolver.Resolve("chrome.exe", "Slack | general").name == "chat");
    CHECK(resolver.Resolve("Code.exe", "main.cpp").name == "code");
    CHECK(resolver.Resolve("OUTLOOK.EXE", "Inbox").name == "email");
    CHECK(resolver.Resolve("chrome.exe", "Gmail - Compose").name == "email");
    CHECK(resolver.Resolve("explorer.exe", "File Explorer").name == "default");
}

TEST_CASE("custom rules take precedence over later ones and invalid regex disables") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_app_profiles.json").string();
    WriteFile(path, R"json({
        "rules": [
            {"match": {"process": "code\\.exe", "title_contains": "REVIEW"}, "profile": "notes"},
            {"match": {"process": "code\\.exe"}, "profile": "code"},
            {"match": {"process": "([unclosed"}, "profile": "chat"}
        ],
        "profiles": {
            "notes": {"prompt_addendum": "review notes", "use_llm": true},
            "code": {"prompt_addendum": "code mode", "trailing_period": false},
            "chat": {"prompt_addendum": "chat mode"}
        }
    })json");
    AppContextResolver resolver;
    CHECK(resolver.Load(path));
    REQUIRE(resolver.Warnings().size() == 1);
    CHECK(resolver.Warnings()[0].find("invalid process regex") != std::string::npos);

    CHECK(resolver.Resolve("code.exe", "PR REVIEW").name == "notes");
    CHECK(resolver.Resolve("code.exe", "main.cpp").name == "code");
    CHECK(resolver.Resolve("notepad.exe", "x").name == "default");
    CHECK(resolver.ProfileByName("chat").prompt_addendum == "chat mode");
}

TEST_CASE("prompt composition appends the profile addendum only") {
    AppProfile chat;
    chat.name = "chat";
    chat.prompt_addendum = "This is a casual chat message.";
    chat.casual = true;
    chat.trailing_period = false;
    const std::string base = "Fix grammar of the dictated text.";
    const std::string composed = AppContextResolver::BuildCleanupPrompt(base, chat);
    CHECK(composed.find(base) == 0);
    CHECK(composed.find("casual chat message") != std::string::npos);
    CHECK(composed.find("Do not add a trailing period") != std::string::npos);

    AppProfile code;
    code.name = "code";
    const std::string code_prompt = AppContextResolver::BuildCleanupPrompt(base, code);
    CHECK(code_prompt.find("No auto-capitalization") != std::string::npos);
}

TEST_CASE("resolver output never contains the window title") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    const AppProfile& profile = resolver.Resolve("slack.exe", "TOP SECRET PROJECT CHANNEL");
    const std::string prompt =
        AppContextResolver::BuildCleanupPrompt("base prompt", profile);
    CHECK(prompt.find("TOP SECRET") == std::string::npos);
    CHECK(prompt.find("slack.exe") == std::string::npos);
}

TEST_CASE("cleanup prompt passes through profile addendum without title leakage") {
    ScriptedLlm llm;
    DictationCleanup cleanup(&llm);
    TextPipeline pipeline(cleanup);
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    const AppProfile& profile = resolver.Resolve("Code.exe", "SECRETKEYS.TXT - editor");
    cleanup.SetPromptAddendum(AppContextResolver::BuildCleanupPrompt("base", profile).substr(4));
    (void)pipeline.Process("one two three four five");
    // DictationCleanup passes the composed prompt as ILlmProvider's system_prompt
    // argument; the fake records what it receives in `prompts`.
    CHECK(llm.prompts.back().find("SECRETKEYS") == std::string::npos);
    CHECK(llm.prompts.back().find("No auto-capitalization") != std::string::npos);
}

TEST_CASE("app context Save round-trips profiles and rules") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_appctx_save.json").string();
    WriteFile(path, R"json({
        "profiles": {
            "p": {"prompt_addendum": "hi", "casual": true, "trailing_period": false,
                  "use_llm": false, "paste_chord": "ctrl+shift+v", "category": "Emails"}
        },
        "rules": [
            {"match": {"process": "foo\\.exe"}, "profile": "p", "disabled": false}
        ]
    })json");
    AppContextResolver resolver;
    REQUIRE(resolver.Load(path));
    CHECK(resolver.Path() == path);
    CHECK(resolver.AssignCategory("p", "Personal"));
    CHECK(resolver.SetEnabled("p", false));
    REQUIRE(resolver.Save());

    AppContextResolver reloaded;
    REQUIRE(reloaded.Load(path));
    CHECK(reloaded.ProfileByName("p").category == "Personal");
    CHECK(reloaded.ProfileByName("p").casual == true);
    CHECK(reloaded.ProfileByName("p").use_llm == false);
    CHECK(reloaded.ProfileByName("p").paste_chord == "ctrl+shift+v");
    // rule disabled -> resolves to default
    CHECK(reloaded.Resolve("foo.exe", "").name == "default");
}

TEST_CASE("app context AssignCategory rejects unknown category/profile") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    CHECK_FALSE(resolver.AssignCategory("nope", "Emails"));
    CHECK_FALSE(resolver.AssignCategory("email", "Not a category"));
    CHECK(resolver.AssignCategory("email", "Personal"));
}

TEST_CASE("app context SetEnabled toggles matching rules") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_appctx_enabled.json").string();
    WriteFile(path, R"json({
        "profiles": {"p": {"prompt_addendum": "hi"}},
        "rules": [
            {"match": {"process": "foo\\.exe"}, "profile": "p", "disabled": false}
        ]
    })json");
    AppContextResolver resolver;
    REQUIRE(resolver.Load(path));
    CHECK(resolver.SetEnabled("p", false));
    CHECK_FALSE(resolver.ProfileEnabled("p"));
    CHECK(resolver.SetEnabled("p", true));
    CHECK(resolver.ProfileEnabled("p"));
    CHECK_FALSE(resolver.SetEnabled("missing", true));
}

TEST_CASE("app context Save drops unknown keys but keeps known data") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_appctx_unknown.json").string();
    WriteFile(path, R"json({
        "profiles": {"p": {"prompt_addendum": "hi", "future_key": 42}},
        "rules": [],
        "unknown_top": "x"
    })json");
    AppContextResolver resolver;
    REQUIRE(resolver.Load(path));
    REQUIRE(resolver.Save());
    const std::string content = [&]() {
        std::ifstream f(path);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    }();
    CHECK(content.find("future_key") == std::string::npos);
    CHECK(content.find("prompt_addendum") != std::string::npos);
    CHECK(content.find("unknown_top") == std::string::npos);
}

TEST_CASE("app context Reload returns false when path empty") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    CHECK_FALSE(resolver.Reload());
    CHECK(resolver.Path().empty());
}
