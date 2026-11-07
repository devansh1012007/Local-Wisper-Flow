#include "core/AppContextResolver.h"
#include "core/StatsStore.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::temp_directory_path() /
            (std::string("lwf_stats_02b_") + name + ".json"))
        .string();
}

std::int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

DictationRecord MakeRecord(std::int64_t ts, const std::string& text,
                           const std::string& process) {
    DictationRecord record;
    record.timestamp_local = ts;
    record.duration_ms = 1000;
    record.final_text = text;
    record.raw_text = text;
    record.app_profile = "default";
    record.process_name = process;
    record.words_corrected = 0;
    record.dictionary_fixes = 0;
    return record;
}

void WriteJson(const std::string& path, const nlohmann::json& json) {
    std::ofstream file(path, std::ios::trunc);
    file << json.dump(4) << '\n';
}

}

TEST_CASE("stats store record and load round trip") {
    const std::string path = TempPath("roundtrip");
    std::filesystem::remove(path);
    const std::int64_t now = NowSeconds();
    {
        StatsStore store(path);
        DictationRecord record = MakeRecord(now, "hello world again", "slack.exe");
        record.duration_ms = 1500;
        record.words_corrected = 1;
        record.dictionary_fixes = 2;
        store.Record(record);
        REQUIRE(store.Generation() == 1);
    }
    StatsStore reloaded(path);
    const auto snap = reloaded.Snapshot();
    const std::string key = StatsStore::DateKey(now);
    REQUIRE(snap.find(key) != snap.end());
    const DayStats& day = snap.at(key);
    CHECK(day.words == 3);
    CHECK(day.dictations == 1);
    CHECK(day.speaking_ms == 1500);
    CHECK(day.words_corrected == 1);
    CHECK(day.dictionary_fixes == 2);
    REQUIRE(day.by_category.find("Work messages") != day.by_category.end());
    CHECK(day.by_category.at("Work messages") == 3);
    REQUIRE(day.apps.find("slack.exe") != day.apps.end());
    CHECK(day.apps.at("slack.exe") == 3);
    std::filesystem::remove(path);
}

TEST_CASE("stats store toggle off is no-op") {
    const std::string path = TempPath("toggle");
    std::filesystem::remove(path);
    StatsStore store(path, true);
    store.SetUsageStatsEnabled(false);
    CHECK_FALSE(store.UsageStatsEnabled());
    store.Record(MakeRecord(NowSeconds(), "hello world", "slack.exe"));
    CHECK(store.Snapshot().empty());
    CHECK(store.Generation() == 0);
    store.SetUsageStatsEnabled(true);
    CHECK(store.UsageStatsEnabled());
    std::filesystem::remove(path);
}

TEST_CASE("stats store ignores empty final text") {
    const std::string path = TempPath("empty");
    std::filesystem::remove(path);
    StatsStore store(path);
    store.Record(MakeRecord(NowSeconds(), "", "slack.exe"));
    store.Record(MakeRecord(NowSeconds(), "   \t\n ", "slack.exe"));
    CHECK(store.Snapshot().empty());
    CHECK(store.Generation() == 0);
    std::filesystem::remove(path);
}

TEST_CASE("stats store evicts older than 730 days") {
    const std::string path = TempPath("evict");
    std::filesystem::remove(path);
    const std::int64_t now = NowSeconds();
    nlohmann::json planted = nlohmann::json::object();
    for (int i = 0; i < 731; ++i) {
        const std::string key = StatsStore::DateKey(now - static_cast<std::int64_t>(i) * 86400);
        nlohmann::json day;
        day["words"] = 1;
        day["dictations"] = 1;
        day["speaking_ms"] = 1000;
        day["words_corrected"] = 0;
        day["dictionary_fixes"] = 0;
        nlohmann::json cats = nlohmann::json::object();
        for (const std::string& name : AppContextResolver::CategoryNames()) {
            cats[name] = 0;
        }
        day["by_category"] = cats;
        nlohmann::json apps = nlohmann::json::object();
        apps["slack.exe"] = 1;
        day["apps"] = apps;
        planted[key] = day;
    }
    WriteJson(path, planted);
    StatsStore store(path);
    const auto snap = store.Snapshot();
    CHECK(snap.size() == 730);
    const std::string oldest = StatsStore::DateKey(now - 730 * 86400);
    CHECK(snap.find(oldest) == snap.end());
    const std::string today = StatsStore::DateKey(now);
    CHECK(snap.find(today) != snap.end());
    std::filesystem::remove(path);
}

TEST_CASE("stats store concurrent records are thread safe") {
    const std::string path = TempPath("threads");
    std::filesystem::remove(path);
    StatsStore store(path);
    const std::int64_t now = NowSeconds();
    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t) {
        workers.emplace_back([&store, now]() {
            for (int i = 0; i < 50; ++i) {
                store.Record(MakeRecord(now, "hello world", "slack.exe"));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    const auto snap = store.Snapshot();
    const std::string key = StatsStore::DateKey(now);
    REQUIRE(snap.find(key) != snap.end());
    CHECK(snap.at(key).dictations == 400);
    CHECK(snap.at(key).words == 800);
    CHECK(snap.at(key).by_category.at("Work messages") == 800);
    std::filesystem::remove(path);
}

TEST_CASE("stats store tolerates corrupt file") {
    const std::string path = TempPath("corrupt");
    std::filesystem::remove(path);
    {
        std::ofstream file(path, std::ios::trunc);
        file << "THIS IS NOT JSON";
    }
    StatsStore store(path);
    CHECK(store.Snapshot().empty());
    store.Record(MakeRecord(NowSeconds(), "fresh start words", "notepad.exe"));
    CHECK(store.Snapshot().size() == 1);
    std::filesystem::remove(path);
}

TEST_CASE("stats word count splits on unicode whitespace") {
    CHECK(StatsStore::CountWords("") == 0);
    CHECK(StatsStore::CountWords("  one   two\tthree\nfour") == 4);
    CHECK(StatsStore::CountWords("hello world") == 2);
}

TEST_CASE("app context category mapping process first title second") {
    CHECK(AppContextResolver::CategoryFor("chatgpt.exe", "") == "AI prompts");
    CHECK(AppContextResolver::CategoryFor("claude.exe", "") == "AI prompts");
    CHECK(AppContextResolver::CategoryFor("cursor.exe", "") == "AI prompts");
    CHECK(AppContextResolver::CategoryFor("gemini.exe", "") == "AI prompts");
    CHECK(AppContextResolver::CategoryFor("perplexity.exe", "") == "AI prompts");
    CHECK(AppContextResolver::CategoryFor("OUTLOOK.EXE", "") == "Emails");
    CHECK(AppContextResolver::CategoryFor("thunderbird.exe", "") == "Emails");
    CHECK(AppContextResolver::CategoryFor("chrome.exe", "Gmail - Inbox") == "Emails");
    CHECK(AppContextResolver::CategoryFor("slack.exe", "") == "Work messages");
    CHECK(AppContextResolver::CategoryFor("ms-teams.exe", "") == "Work messages");
    CHECK(AppContextResolver::CategoryFor("whatsapp.exe", "") == "Personal");
    CHECK(AppContextResolver::CategoryFor("telegram.exe", "") == "Personal");
    CHECK(AppContextResolver::CategoryFor("messages.exe", "") == "Personal");
    CHECK(AppContextResolver::CategoryFor("winword.exe", "") == "Documents");
    CHECK(AppContextResolver::CategoryFor("notion.exe", "") == "Documents");
    CHECK(AppContextResolver::CategoryFor("obsidian.exe", "") == "Documents");
    CHECK(AppContextResolver::CategoryFor("chrome.exe", "Google Docs - Untitled") ==
          "Documents");
    CHECK(AppContextResolver::CategoryFor("explorer.exe", "File Explorer") == "Other tasks");
    CHECK(AppContextResolver::CategoryFor("slack.exe", "Gmail - Inbox") == "Work messages");
    CHECK(AppContextResolver::CategoryFor("chrome.exe", "ChatGPT") == "AI prompts");
}

TEST_CASE("app context built-in profiles carry categories and load preserves them") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    CHECK(resolver.ProfileByName("email").category == "Emails");
    CHECK(resolver.ProfileByName("chat").category == "Work messages");
    CHECK(resolver.ProfileByName("notes").category == "Documents");
    CHECK(resolver.ProfileByName("code").category == "Other tasks");
    CHECK(resolver.ProfileByName("default").category == "Other tasks");
    CHECK(AppContextResolver::CategoryNames().size() == 6);

    const std::string path = TempPath("profiles");
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{\"profiles\":{\"custom\":{\"category\":\"Emails\",\"prompt_addendum\":\"hi\"},"
                "\"odd\":{\"category\":\"Custom Thing\"}},\"rules\":[]}";
    }
    AppContextResolver loaded;
    CHECK(loaded.Load(path));
    CHECK(loaded.ProfileByName("custom").category == "Emails");
    CHECK(loaded.ProfileByName("odd").category == "Custom Thing");
    std::filesystem::remove(path);
}

TEST_CASE("stats store attributes by category and app") {
    const std::string path = TempPath("attr");
    std::filesystem::remove(path);
    StatsStore store(path);
    const std::int64_t now = NowSeconds();
    store.Record(MakeRecord(now, "one two", "OUTLOOK.EXE"));
    store.Record(MakeRecord(now, "three", "explorer.exe"));
    const auto snap = store.Snapshot();
    const std::string key = StatsStore::DateKey(now);
    REQUIRE(snap.find(key) != snap.end());
    const DayStats& day = snap.at(key);
    CHECK(day.words == 3);
    CHECK(day.by_category.at("Emails") == 2);
    CHECK(day.by_category.at("Other tasks") == 1);
    CHECK(day.apps.at("outlook.exe") == 2);
    CHECK(day.apps.at("explorer.exe") == 1);
    std::filesystem::remove(path);
}
