#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Onboarding {

struct DownloadProgress {
    std::uint64_t downloaded = 0;
    std::uint64_t total = 0;
};

class WindowsDownloader {
public:
    bool Download(const std::string& url, const std::string& destination_path,
                  const std::string& expected_sha256, std::int64_t remote_size,
                  const std::function<bool(const DownloadProgress&)>& progress_callback);

    static std::int64_t QueryRemoteSize(const std::string& url);
};

}
