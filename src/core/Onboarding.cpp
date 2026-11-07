#include "core/Onboarding.h"
#include "core/Sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Onboarding {

ModelCatalog ParseModelsJson(const std::string& path) {
    ModelCatalog catalog;
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) {
        catalog.warnings.push_back("models.json not found at " + path);
        return catalog;
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        catalog.warnings.push_back("models.json not found at " + path);
        return catalog;
    }
    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        catalog.warnings.push_back(std::string("models.json parse failed: ") + e.what());
        return catalog;
    }
    if (!json.is_object()) {
        catalog.warnings.push_back("models.json root must be an object");
        return catalog;
    }

    const auto read_section = [&catalog](const nlohmann::json& json, const char* key,
                                         std::vector<ModelDownload>& out) {
        if (!json.contains(key) || !json[key].is_array()) {
            return;
        }
        for (const auto& element : json[key]) {
            if (!element.is_object() || !element.contains("name") || !element.contains("url")) {
                continue;
            }
            ModelDownload model;
            model.name = element["name"].get<std::string>();
            model.url = element["url"].get<std::string>();
            if (element.contains("sha256") && element["sha256"].is_string()) {
                model.sha256 = element["sha256"].get<std::string>();
            }
            if (element.contains("size_bytes") && element["size_bytes"].is_number_unsigned()) {
                model.size_bytes = element["size_bytes"].get<std::uint64_t>();
            }
            if (model.sha256.empty()) {
                catalog.warnings.push_back("model '" + model.name + "' has no sha256");
            }
            out.push_back(std::move(model));
        }
    };

    read_section(json, "whisper", catalog.whisper);
    read_section(json, "llm", catalog.llm);
    return catalog;
}

ResumePlan PlanResume(std::uint64_t existing_bytes, std::uint64_t remote_size,
                      const std::string& expected_sha256, const std::string& actual_sha256) {
    ResumePlan plan;

    if (remote_size == 0) {
        plan.error = "remote size unknown, downloading from scratch";
        return plan;
    }
    if (existing_bytes > remote_size) {
        plan.error = "existing file is larger than the remote file";
        return plan;
    }
    if (existing_bytes == remote_size) {
        if (!expected_sha256.empty() && expected_sha256 != actual_sha256) {
            plan.error = "completed file failed checksum, restarting download";
            return plan;
        }
        plan.valid = true;
        plan.start_offset = 0;
        plan.verify_required = false;
        return plan;
    }
    if (existing_bytes > 0) {
        plan.valid = true;
        plan.start_offset = existing_bytes;
        plan.verify_required = !expected_sha256.empty();
        return plan;
    }

    plan.valid = true;
    plan.start_offset = 0;
    plan.verify_required = !expected_sha256.empty();
    return plan;
}

}
