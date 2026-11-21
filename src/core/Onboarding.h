#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Onboarding {

struct ModelDownload {
    std::string name;
    std::string url;
    std::string sha256;
    std::uint64_t size_bytes = 0;
};

struct ModelCatalog {
    std::vector<ModelDownload> whisper;
    std::vector<ModelDownload> llm;
    std::vector<std::string> warnings;
};

ModelCatalog ParseModelsJson(const std::string& path);

struct ResumePlan {
    bool valid = false;
    std::uint64_t start_offset = 0;
    bool verify_required = true;
    std::string error;
};

ResumePlan PlanResume(std::uint64_t existing_bytes, std::uint64_t remote_size,
                      const std::string& expected_sha256, const std::string& actual_sha256);

}
