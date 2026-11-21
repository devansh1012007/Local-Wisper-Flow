#include "os/DataDir.h"

#include "core/PathResolver.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

namespace {

std::string TrimTrailingSeparator(std::string path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    return path;
}

}

std::string GetDataDir() {
#ifdef _WIN32
    const char* override_dir = std::getenv("LWF_HOME");
    if (override_dir != nullptr && *override_dir != '\0') {
        return TrimTrailingSeparator(std::string(override_dir));
    }
    PWSTR roaming = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming) != S_OK ||
        roaming == nullptr) {
        return std::string(".");
    }
    std::filesystem::path dir = std::filesystem::path(roaming) / "LocalWisperFlow";
    CoTaskMemFree(roaming);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir.string();
#else
    const char* override_dir = std::getenv("LWF_HOME");
    if (override_dir != nullptr && *override_dir != '\0') {
        return TrimTrailingSeparator(std::string(override_dir));
    }
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::filesystem::path base;
    if (xdg != nullptr && *xdg != '\0') {
        base = std::filesystem::path(xdg);
    } else {
        const char* home = std::getenv("HOME");
        if (home == nullptr) {
            return std::string(".");
        }
        base = std::filesystem::path(home) / ".local" / "share";
    }
    std::filesystem::path dir = base / "LocalWisperFlow";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir.string();
#endif
}

std::string GetExeDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0) {
        return std::string(".");
    }
    return std::filesystem::path(std::string(buffer, length)).parent_path().string();
#else
    char buffer[4096];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) {
        return std::string(".");
    }
    buffer[length] = '\0';
    return std::filesystem::path(std::string(buffer)).parent_path().string();
#endif
}

std::string ResolveModelPath(const std::string& configured_path) {
    return ResolveDataPath(configured_path, GetDataDir(), GetExeDir());
}
