#include "core/AtomicFile.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_test_atomic_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

}

TEST_CASE("WriteAtomic writes the full content and leaves no tmp behind") {
    const std::string dir = TempDir("roundtrip");
    const std::string path = (std::filesystem::path(dir) / "notes.txt").string();

    CHECK(lwf::WriteAtomic(path, "hello\nworld\n"));
    CHECK(ReadFile(path) == "hello\nworld\n");
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}

TEST_CASE("WriteAtomic overwrites existing content") {
    const std::string dir = TempDir("overwrite");
    const std::string path = (std::filesystem::path(dir) / "data.json").string();

    WriteFile(path, "old value");
    CHECK(lwf::WriteAtomic(path, "new value"));
    CHECK(ReadFile(path) == "new value");

    CHECK(lwf::WriteAtomic(path, ""));
    CHECK(ReadFile(path).empty());
}

TEST_CASE("WriteAtomic ignores a stale tmp left by an interrupted write") {
    const std::string dir = TempDir("stale");
    const std::string path = (std::filesystem::path(dir) / "config.json").string();

    WriteFile(path, "committed");
    WriteFile(path + ".tmp", "partial garbage from a crash");

    CHECK(lwf::WriteAtomic(path, "next committed"));
    CHECK(ReadFile(path) == "next committed");
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}

TEST_CASE("WriteAtomic fails when the target path is a directory and cleans its tmp") {
    const std::string dir = TempDir("isdir");
    const std::string path = (std::filesystem::path(dir) / "config.json").string();
    std::filesystem::create_directories(path);

    CHECK_FALSE(lwf::WriteAtomic(path, "content"));
    CHECK(std::filesystem::is_directory(path));
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}

TEST_CASE("WriteAtomic preserves binary content with NUL bytes") {
    const std::string dir = TempDir("binary");
    const std::string path = (std::filesystem::path(dir) / "blob.bin").string();
    const std::string content("a\0b\0c", 5);

    CHECK(lwf::WriteAtomic(path, content));
    CHECK(ReadFile(path) == content);
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}

TEST_CASE("WriteAtomic into a missing directory fails and creates nothing") {
    const std::string dir = TempDir("nodir");
    const std::string path = (std::filesystem::path(dir) / "absent" / "file.txt").string();

    CHECK_FALSE(lwf::WriteAtomic(path, "content"));
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
}
