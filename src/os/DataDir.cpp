#include "os/DataDir.h"

#include "core/Log.h"
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

constexpr char kLegacyDirName[] = "LocalWisperFlow";
constexpr char kDataDirName[] = "EchoFlow";

std::string TrimTrailingSeparator(std::string path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    return path;
}

// Branding rename (followups-6): the data directory moved from
// LocalWisperFlow to EchoFlow. One-time migration renames the old
// directory when the new one does not exist yet, so existing installs keep
// config.json, dictionary, snippets, history, logs and downloaded models.
// A failed rename (locked file, old app still running, cross-device) is
// non-fatal: the legacy directory keeps being used for that session and
// the next start retries. LWF_HOME overrides skip migration entirely -
// they are a developer/test escape hatch, not a user location.
std::filesystem::path MigrateDataDir(std::filesystem::path base) {
    const std::filesystem::path legacy = base / kLegacyDirName;
    const std::filesystem::path target = base / kDataDirName;
    std::error_code ec;
    if (std::filesystem::exists(legacy, ec) && !std::filesystem::exists(target, ec)) {
        std::filesystem::rename(legacy, target, ec);
        if (!ec) {
            lwf::Log::Info("DataDir: migrated " + legacy.string() + " -> " + target.string());
        } else {
            lwf::Log::Info("DataDir: rename " + legacy.string() + " failed (" + ec.message() +
                           "); using legacy location this session");
            return legacy;
        }
    }
    // Review followup: when BOTH directories exist (the user restored a
    // backup next to a fresh install, or a failed rename was resolved
    // manually), the new location wins silently. Log the situation so it is
    // visible in the log file instead of looking like lost data - the legacy
    // folder is stale and safe to delete once the new one is confirmed good.
    std::error_code both_ec;
    if (std::filesystem::exists(legacy, both_ec) && std::filesystem::exists(target, both_ec)) {
        lwf::Log::Warning("DataDir: both " + legacy.string() + " and " + target.string() +
                          " exist; using " + target.string() + " and ignoring the legacy "
                          "folder. After checking the new location has your config and models, "
                          "the legacy folder can be deleted manually.");
    }
    return target;
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
    std::filesystem::path dir = MigrateDataDir(std::filesystem::path(roaming));
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
    std::filesystem::path dir = MigrateDataDir(base);
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
