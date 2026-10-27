#include <doctest/doctest.h>

#include "os/AppPaths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class EnvGuard {
public:
    explicit EnvGuard(const char* name) : name_(name) {
        const char* existing = std::getenv(name);
        had_value_ = existing != nullptr;
        if (had_value_) {
            value_ = existing;
        }
    }

    ~EnvGuard() {
        if (had_value_) {
#ifdef _WIN32
            _putenv_s(name_.c_str(), value_.c_str());
#else
            setenv(name_.c_str(), value_.c_str(), 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s(name_.c_str(), "");
#else
            unsetenv(name_.c_str());
#endif
        }
    }

    void Set(const std::string& value) {
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    void Clear() {
#ifdef _WIN32
        _putenv_s(name_.c_str(), "");
#else
        unsetenv(name_.c_str());
#endif
    }

private:
    std::string name_;
    std::string value_;
    bool had_value_ = false;
};

void WriteFile(const std::string& path, const std::string& body) {
    std::ofstream file(path);
    file << body;
}

}  // namespace

TEST_CASE("exe dir points at the running binary") {
    const std::string exe_dir = AppPaths::ExeDir();
    CHECK(AppPaths::FileExists(AppPaths::JoinPath(exe_dir, "lwf-tests")));
}

TEST_CASE("data dir honors LWF_HOME") {
    EnvGuard guard("LWF_HOME");
    const std::string temp = (std::filesystem::temp_directory_path() / "lwf-data-test").string();
    std::filesystem::create_directories(temp);
    guard.Set(temp);
    CHECK(AppPaths::DataDir() == temp);
}

TEST_CASE("resolve config prefers LWF_HOME then data dir") {
    EnvGuard guard("LWF_HOME");
    const std::string base = (std::filesystem::temp_directory_path() / "lwf-cfg-test").string();
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    guard.Set(base);
    const std::string resolved = AppPaths::ResolveConfigPath();
    CHECK(resolved == AppPaths::JoinPath(base, "config.json"));

    WriteFile(resolved, "{}");
    CHECK(AppPaths::ResolveConfigPath() == resolved);
}

TEST_CASE("resolve asset finds files next to the exe") {
    const std::string exe_dir = AppPaths::ExeDir();
    const std::string name = "apppaths_probe.txt";
    const std::string probe = AppPaths::JoinPath(exe_dir, name);
    WriteFile(probe, "x");
    const std::string resolved = AppPaths::ResolveAsset(name);
    CHECK(AppPaths::FileExists(resolved));
    CHECK(resolved.find(name) != std::string::npos);
    std::filesystem::remove(probe);
    CHECK(AppPaths::ResolveAsset("definitely_missing_asset.bin") ==
          "definitely_missing_asset.bin");
}

TEST_CASE("resolve asset keeps absolute paths untouched") {
#ifdef _WIN32
    CHECK(AppPaths::ResolveAsset("C:\\models\\model.bin") == "C:\\models\\model.bin");
#else
    CHECK(AppPaths::ResolveAsset("/opt/models/model.bin") == "/opt/models/model.bin");
#endif
}

TEST_CASE("resolve asset honors LWF_HOME for relative paths") {
    EnvGuard guard("LWF_HOME");
    const std::string base = (std::filesystem::temp_directory_path() / "lwf-asset-test").string();
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    guard.Set(base);
    WriteFile(AppPaths::JoinPath(base, "model.bin"), "x");
    CHECK(AppPaths::ResolveAsset("model.bin") == AppPaths::JoinPath(base, "model.bin"));
}
