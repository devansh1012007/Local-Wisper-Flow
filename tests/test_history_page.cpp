#include "ui/pages/HistoryPage.h"

#include "core/HistoryStore.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::temp_directory_path() /
            (std::string("lwf_histpage_") + name + ".jsonl"))
        .string();
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

}  // namespace

TEST_CASE("history page visible entries search substring") {
    const std::string path = TempPath("search");
    std::filesystem::remove(path);
    HistoryStore store(path, true, false, 50, 90);
    HistoryEntry a;
    a.timestamp = 100;
    a.text = "alpha meeting notes";
    a.app_profile = "default";
    a.duration_ms = 100;
    store.Append(a);
    HistoryEntry b;
    b.timestamp = 200;
    b.text = "beta grocery list";
    b.app_profile = "default";
    b.duration_ms = 100;
    store.Append(b);

    const auto all = lwf::ui::pages::VisibleEntries(store, "", 10);
    CHECK(all.size() == 2);
    CHECK(all[0].text == "beta grocery list");

    const auto hits = lwf::ui::pages::VisibleEntries(store, "grocery", 10);
    CHECK(hits.size() == 1);
    CHECK(hits[0].text == "beta grocery list");
}

TEST_CASE("history page format timestamp") {
    CHECK(lwf::ui::pages::FormatTimestamp(0) == "\xE2\x80\x94");
    const std::string formatted = lwf::ui::pages::FormatTimestamp(1700000000);
    CHECK(formatted.size() == 16);
}

TEST_CASE("history page sources stay on palette and tokens") {
    const std::string header = ReadFile("src/ui/pages/HistoryPage.h");
    const std::string impl = ReadFile("src/ui/pages/HistoryPage.cpp");
    REQUIRE_FALSE(header.empty());
    REQUIRE_FALSE(impl.empty());
    CHECK(header.find("Draw(float scale, const lwf::ui::Palette") != std::string::npos);
    CHECK(impl.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("palette.") != std::string::npos);
    CHECK(impl.find("Color(palette.bg)") != std::string::npos);
    CHECK(impl.find("Pack(palette.border)") != std::string::npos);
    CHECK(impl.find("Pack(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.text)") != std::string::npos);
    CHECK(impl.find("Color(palette.err)") != std::string::npos);
    CHECK(impl.find("LightButton") != std::string::npos);
    CHECK(impl.find("No history yet.") != std::string::npos);
    CHECK(impl.find("Retention: Settings > Privacy") != std::string::npos);
    CHECK(impl.find("Export...") != std::string::npos);
    CHECK(impl.find("Open privacy settings") != std::string::npos);
    CHECK(impl.find("BeginCard") == std::string::npos);
}