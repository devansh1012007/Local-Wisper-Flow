#include "ui/pages/ProfilesPage.h"

#include "core/AppContextResolver.h"

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
            (std::string("lwf_profpage_") + name + ".json"))
        .string();
}
std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}
}  // namespace

TEST_CASE("profiles page profile names sorted") {
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    const auto names = lwf::ui::pages::ProfileNames(resolver);
    CHECK(names.size() >= 4);
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    CHECK(names == sorted);
}

TEST_CASE("profiles page category index") {
    CHECK(lwf::ui::pages::CategoryIndex("Emails") >= 0);
    CHECK(lwf::ui::pages::CategoryIndex("Nope") == 5);
}

TEST_CASE("profiles page sources stay on palette and tokens") {
    const std::string header = ReadFile("src/ui/pages/ProfilesPage.h");
    const std::string impl = ReadFile("src/ui/pages/ProfilesPage.cpp");
    REQUIRE_FALSE(header.empty());
    REQUIRE_FALSE(impl.empty());
    CHECK(header.find("Draw(float scale, const lwf::ui::Palette") != std::string::npos);
    CHECK(impl.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("palette.") != std::string::npos);
    CHECK(impl.find("Pack(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.border)") != std::string::npos);
    CHECK(impl.find("Pack(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.text)") != std::string::npos);
    CHECK(impl.find("widgets::Row") != std::string::npos);
    CHECK(impl.find("BeginCard") == std::string::npos);
    CHECK(impl.find("No profiles.") != std::string::npos);
    CHECK(impl.find("Applies after restart.") != std::string::npos);
}