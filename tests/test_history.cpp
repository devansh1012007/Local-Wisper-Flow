#include "core/HistoryStore.h"

#include <doctest/doctest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::path("/tmp") / (std::string("lwf_hist_") + name + ".jsonl")).string();
}

std::string ReadAll(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

HistoryEntry MakeEntry(std::int64_t ts, std::string text) {
    HistoryEntry entry;
    entry.timestamp = ts;
    entry.text = std::move(text);
    entry.app_profile = "default";
    entry.duration_ms = 1500;
    entry.language = "en";
    return entry;
}

}

TEST_CASE("append, search, last and clear") {
    const std::string path = TempPath("ops");
    std::filesystem::remove(path);
    HistoryStore history(path, true, false, 100, 90);

    history.Append(MakeEntry(NowSeconds() - 300, "alpha meeting notes"));
    history.Append(MakeEntry(NowSeconds() - 200, "beta grocery list"));
    history.Append(MakeEntry(NowSeconds() - 100, "gamma standup"));
    REQUIRE(history.Size() == 3);

    const auto last = history.Last();
    REQUIRE(last.has_value());
    CHECK(last->text == "gamma standup");

    const auto hits = history.Search("standup", 10);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].timestamp == NowSeconds() - 100);

    const auto limited = history.Search("", 2);
    CHECK(limited.size() == 2);
    CHECK(limited[0].text == "gamma standup");
    CHECK(limited[1].text == "beta grocery list");

    history.Clear();
    CHECK(history.Size() == 0);
    CHECK(ReadAll(path).empty());
}

TEST_CASE("raw text only stored when keep_raw is enabled") {
    const std::string keep_path = TempPath("keep");
    std::filesystem::remove(keep_path);
    HistoryEntry entry = MakeEntry(100, "clean text");
    entry.raw = "uhh the clean text";
    {
        HistoryStore history(keep_path, true, true, 50, 90);
        history.Append(entry);
    }
    CHECK(ReadAll(keep_path).find("uhh the clean text") != std::string::npos);

    const std::string strip_path = TempPath("strip");
    std::filesystem::remove(strip_path);
    {
        HistoryStore history(strip_path, true, false, 50, 90);
        history.Append(entry);
    }
    CHECK(ReadAll(strip_path).find("uhh") == std::string::npos);
    CHECK(ReadAll(strip_path).find("clean text") != std::string::npos);
}

TEST_CASE("retention caps entries and age on startup") {
    const std::string path = TempPath("retention");
    std::filesystem::remove(path);
    {
        HistoryStore history(path, true, false, 3, 90);
        for (int i = 0; i < 6; ++i) {
            history.Append(MakeEntry(NowSeconds() - 100 + i, "entry " + std::to_string(i)));
        }
    }
    {
        HistoryStore history(path, true, false, 3, 90);
        CHECK(history.Size() == 3);
        const auto last = history.Last();
        REQUIRE(last.has_value());
        CHECK(last->text == "entry 5");
    }

    const std::string aged_path = TempPath("aged");
    std::filesystem::remove(aged_path);
    {
        HistoryStore history(aged_path, true, false, 50, 90);
        HistoryEntry old = MakeEntry(1000, "ancient text");
        old.timestamp = 1000;
        history.Append(old);
        HistoryEntry fresh = MakeEntry(NowSeconds(), "fresh text");
        history.Append(fresh);
    }
    HistoryStore reloaded(aged_path, true, false, 50, 90);
    REQUIRE(reloaded.Size() == 1);
    CHECK(reloaded.Last()->text == "fresh text");
}

TEST_CASE("corrupt lines are skipped and appends continue") {
    const std::string path = TempPath("corrupt");
    std::filesystem::remove(path);
    {
        std::ofstream file(path);
        file << "{\"ts\":" << (NowSeconds() - 50) << ",\"text\":\"good line\"}\n";
        file << "THIS IS NOT JSON\n";
        file << "\n";
        file << "{\"ts\":" << (NowSeconds() - 40) << ",\"text\":\"another good\"}\n";
    }
    HistoryStore history(path, true, false, 50, 90);
    REQUIRE(history.Size() == 2);
    history.Append(MakeEntry(NowSeconds() - 30, "third"));
    CHECK(history.Size() == 3);
    CHECK(history.Search("third", 5).size() == 1);
}

TEST_CASE("pause switch stops appends") {
    const std::string path = TempPath("paused");
    std::filesystem::remove(path);
    HistoryStore history(path, true, false, 50, 90);
    history.SetEnabled(false);
    history.Append(MakeEntry(NowSeconds() - 10, "ignored"));
    CHECK(history.Size() == 0);
    CHECK_FALSE(history.Enabled());
    history.SetEnabled(true);
    history.Append(MakeEntry(NowSeconds(), "kept"));
    CHECK(history.Size() == 1);
}

TEST_CASE("export writes a readable report") {
    const std::string path = TempPath("export_src");
    std::filesystem::remove(path);
    HistoryStore history(path, true, false, 50, 90);
    history.Append(MakeEntry(NowSeconds(), "exported sentence"));

    const std::string export_path = TempPath("export_out");
    CHECK(history.Export(export_path));
    const std::string content = ReadAll(export_path);
    CHECK(content.find("exported sentence") != std::string::npos);
    CHECK(content.find("default") != std::string::npos);
}
