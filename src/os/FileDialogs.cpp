#include "os/FileDialogs.h"

#include "os/DataDir.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#ifdef _WIN32
namespace {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size) <= 0) {
        return {};
    }
    result.pop_back();
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr) <= 0) {
        return {};
    }
    result.pop_back();
    return result;
}

}  // namespace

#endif

namespace FileDialogs {

std::string PickModelFile(const std::string& current_path, void* owner_hwnd) {
#ifdef _WIN32
    const std::string initial_dir = InitialDirForModel(current_path, GetDataDir());
    const std::wstring filter = BuildModelFileFilter();
    const std::wstring initial_wide = Utf8ToWide(initial_dir);
    std::vector<wchar_t> file_buffer(32768, L'\0');

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(owner_hwnd);
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = file_buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(file_buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    dialog.lpstrTitle = L"Select model file";
    if (!initial_wide.empty()) {
        dialog.lpstrInitialDir = initial_wide.c_str();
    }
    if (GetOpenFileNameW(&dialog) == 0) {
        return {};
    }
    return WideToUtf8(std::wstring(file_buffer.data()));
#else
    (void)current_path;
    (void)owner_hwnd;
    return {};
#endif
}

std::string PickModelFile(const std::string& current_path) {
    return PickModelFile(current_path, nullptr);
}

std::string NormalizeModelPath(const std::string& path) {
    std::string normalized = path;
    const std::size_t first = normalized.find_first_not_of(" \t\r\n");
    const std::size_t last = normalized.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    normalized = normalized.substr(first, last - first + 1);
    if (normalized.size() >= 2 && normalized.front() == '"' && normalized.back() == '"') {
        normalized = normalized.substr(1, normalized.size() - 2);
    }
    for (char& ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return normalized;
}

std::string InitialDirForModel(const std::string& current_path, const std::string& data_dir) {
    if (current_path.empty()) {
        return data_dir;
    }
    const std::filesystem::path parent = std::filesystem::path(current_path).parent_path();
    if (parent.empty()) {
        return data_dir;
    }
    return parent.string();
}

#ifdef _WIN32
std::wstring BuildModelFileFilter() {
    std::wstring filter = L"Model files (*.gguf;*.bin)";
    filter.push_back(L'\0');
    filter += L"*.gguf;*.bin";
    filter.push_back(L'\0');
    filter += L"All files (*.*)";
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    return filter;
}
#else
std::string BuildModelFileFilter() {
    std::string filter = "Model files (*.gguf;*.bin)";
    filter.push_back('\0');
    filter += "*.gguf;*.bin";
    filter.push_back('\0');
    filter += "All files (*.*)";
    filter.push_back('\0');
    filter += "*.*";
    filter.push_back('\0');
    filter.push_back('\0');
    return filter;
}
#endif

}  // namespace FileDialogs
