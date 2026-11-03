#pragma once

#include <string>

std::string GetDataDir();
std::string GetExeDir();
std::string ResolveModelPath(const std::string& configured_path);
