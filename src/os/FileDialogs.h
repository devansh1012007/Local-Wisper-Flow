#pragma once

#include <string>

namespace FileDialogs {

std::string PickModelFile(const std::string& current_path, void* owner_hwnd);
std::string PickModelFile(const std::string& current_path);

std::string NormalizeModelPath(const std::string& path);
std::string InitialDirForModel(const std::string& current_path, const std::string& data_dir);

#ifdef _WIN32
std::wstring BuildModelFileFilter();
#else
std::string BuildModelFileFilter();
#endif

}  // namespace FileDialogs
