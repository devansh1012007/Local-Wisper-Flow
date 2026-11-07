#include "core/AtomicFile.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace lwf {

bool WriteAtomic(const std::string& path, const std::string& content) {
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream tmp(tmp_path, std::ios::trunc);
        if (!tmp.is_open()) {
            return false;
        }
        tmp << content;
        tmp.flush();
        if (!tmp.good()) {
            tmp.close();
            std::error_code cleanup_ec;
            std::filesystem::remove(tmp_path, cleanup_ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove(tmp_path, cleanup_ec);
        return false;
    }
    return true;
}

}
