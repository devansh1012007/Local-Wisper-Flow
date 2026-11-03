#include "core/CrashReporter.h"

#include "core/AppVersion.h"
#include "core/Log.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace lwf {

std::string CrashReporter::WriteCrashReport(const std::string& logs_dir,
                                            const std::string& exception_code,
                                            const std::string& fsm_state,
                                            const std::string& detail) {
    std::error_code ec;
    std::filesystem::create_directories(logs_dir, ec);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const std::string path =
        (std::filesystem::path(logs_dir) / ("crash-" + std::to_string(now_ms) + ".txt")).string();

    std::ostringstream out;
    out << "app: Local Wisper Flow " << AppVersion() << "\n";
    out << "exception: " << exception_code << "\n";
    out << "fsm_state: " << fsm_state << "\n";
    if (!detail.empty()) {
        out << "detail: " << detail << "\n";
    }
    out << "recent log lines:\n";
    for (const std::string& line : Log::RecentLinesTry(200)) {
        out << line << "\n";
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
        file << out.str();
    }
    return path;
}

bool CrashReporter::HasCrashReports(const std::string& logs_dir) {
    return !FindUnseenCrash(logs_dir).empty();
}

std::string CrashReporter::FindUnseenCrash(const std::string& logs_dir) {
    std::error_code ec;
    std::filesystem::directory_iterator it(logs_dir, ec);
    if (ec) {
        return {};
    }
    std::string best;
    for (const std::filesystem::directory_entry& entry : it) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind("crash-", 0) == 0 && name.size() > 10 &&
            name.compare(name.size() - 4, 4, ".txt") == 0) {
            const std::string stem = name.substr(6, name.size() - 10);
            bool digits_only = !stem.empty();
            for (const char c : stem) {
                if (c < '0' || c > '9') {
                    digits_only = false;
                    break;
                }
            }
            if (digits_only && (best.empty() || name > best)) {
                best = name;
            }
        }
    }
    if (best.empty()) {
        return {};
    }
    return (std::filesystem::path(logs_dir) / best).string();
}

bool CrashReporter::MarkSeen(const std::string& crash_path) {
    std::error_code ec;
    if (!std::filesystem::exists(crash_path, ec)) {
        return false;
    }
    const std::filesystem::path from(crash_path);
    std::filesystem::path to = from;
    to.replace_extension(".seen.txt");
    std::filesystem::rename(from, to, ec);
    return !ec;
}

}
