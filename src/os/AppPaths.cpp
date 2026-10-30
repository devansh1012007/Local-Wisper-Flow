#include "os/AppPaths.h"

#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

std::string ParentOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return {};
    }
    return path.substr(0, slash);
}

std::string ReadEnv(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : std::string();
}

}  // namespace

namespace AppPaths {

bool FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

void EnsureDirectory(const std::string& path) {
    if (path.empty()) {
        return;
    }
#ifdef _WIN32
    CreateDirectoryA(path.c_str(), nullptr);
#else
    mkdir(path.c_str(), 0755);
#endif
}

std::string JoinPath(const std::string& base, const std::string& name) {
    if (base.empty()) {
        return name;
    }
    const char last = base.back();
    if (last == '/' || last == '\\') {
        return base + name;
    }
    return base + "/" + name;
}

#ifdef _WIN32

std::string ExeDir() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return ".";
    }
    const std::string dir = ParentOf(std::string(buffer, length));
    return dir.empty() ? std::string(".") : dir;
}

std::string DataDir() {
    const std::string override_dir = ReadEnv("LWF_HOME");
    if (!override_dir.empty()) {
        return override_dir;
    }
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        const std::string base = JoinPath(path, "LocalWisperFlow");
        EnsureDirectory(base);
        return base;
    }
    return ExeDir();
}

#else

std::string ExeDir() {
    const std::string self = "/proc/self/exe";
    std::string buffer(4096, '\0');
    const ssize_t length = readlink(self.c_str(), buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return ".";
    }
    buffer.resize(static_cast<std::size_t>(length));
    const std::string dir = ParentOf(buffer);
    return dir.empty() ? std::string(".") : dir;
}

std::string DataDir() {
    const std::string override_dir = ReadEnv("LWF_HOME");
    if (!override_dir.empty()) {
        return override_dir;
    }
    const std::string home = ReadEnv("HOME");
    if (home.empty()) {
        return ExeDir();
    }
    const std::string base = JoinPath(home, ".local/share/LocalWisperFlow");
    EnsureDirectory(base);
    return base;
}

#endif

std::string ResolveConfigPath() {
    const std::string override_dir = ReadEnv("LWF_HOME");
    if (!override_dir.empty()) {
        const std::string candidate = JoinPath(override_dir, "config.json");
        if (FileExists(candidate)) {
            return candidate;
        }
    }
    const std::string data_dir = DataDir();
    const std::string in_data = JoinPath(data_dir, "config.json");
    if (FileExists(in_data)) {
        return in_data;
    }
    const std::string in_exe = JoinPath(ExeDir(), "config.json");
    if (FileExists(in_exe)) {
        return in_exe;
    }
    if (FileExists("config.json")) {
        return "config.json";
    }
    if (!override_dir.empty()) {
        EnsureDirectory(override_dir);
        return JoinPath(override_dir, "config.json");
    }
    return in_data;
}

std::string ResolveAsset(const std::string& configured_path) {
    if (configured_path.empty()) {
        return configured_path;
    }
    const bool absolute =
        (configured_path.size() >= 2 && configured_path[1] == ':') ||
        (!configured_path.empty() && (configured_path[0] == '/' || configured_path[0] == '\\'));
    if (absolute) {
        return configured_path;
    }
    const std::string in_exe = JoinPath(ExeDir(), configured_path);
    if (FileExists(in_exe)) {
        return in_exe;
    }
    if (FileExists(configured_path)) {
        return configured_path;
    }
    const std::string in_data = JoinPath(DataDir(), configured_path);
    if (FileExists(in_data)) {
        return in_data;
    }
    const std::string in_models = JoinPath(JoinPath(DataDir(), "models"), configured_path);
    if (FileExists(in_models)) {
        return in_models;
    }
    return configured_path;
}

}  // namespace AppPaths
