#pragma once

#include <string>

namespace AppPaths {

std::string ExeDir();

std::string DataDir();

std::string JoinPath(const std::string& base, const std::string& name);

bool FileExists(const std::string& path);

std::string ResolveConfigPath();

std::string ResolveAsset(const std::string& configured_path);

void EnsureDirectory(const std::string& path);

}  // namespace AppPaths
