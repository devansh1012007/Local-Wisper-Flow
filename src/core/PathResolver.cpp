#include "core/PathResolver.h"

#include <filesystem>

std::string ResolveDataPath(const std::string& path, const std::string& data_dir,
                            const std::string& exe_dir) {
    if (path.empty()) {
        return path;
    }
    const std::filesystem::path candidate(path);
    if (candidate.is_absolute()) {
        return path;
    }
    if (!data_dir.empty() && std::filesystem::exists(std::filesystem::path(data_dir) / candidate)) {
        return (std::filesystem::path(data_dir) / candidate).string();
    }
    if (!exe_dir.empty() && std::filesystem::exists(std::filesystem::path(exe_dir) / candidate)) {
        return (std::filesystem::path(exe_dir) / candidate).string();
    }
    // Fall back to current working directory (e.g. repo root when run from there).
    if (std::filesystem::exists(candidate)) {
        return candidate.string();
    }
    return path;
}
