#include "core/Onboarding.h"
#include "core/Sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
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
            // Optional display metadata (ModelsPage); tolerate wrong types
            // by leaving the default empty string.
            const auto read_string_field = [&element](const char* key) {
                if (element.contains(key) && element[key].is_string()) {
                    return element[key].get<std::string>();
                }
                return std::string();
            };
            model.display_name = read_string_field("display_name");
            model.language = read_string_field("language");
            model.license = read_string_field("license");
            model.notes = read_string_field("notes");
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

DownloadError MapDownloadError(ErrorCode code, const std::string& detail) {
    DownloadError error;
    error.detail = detail;
    switch (code) {
        case ErrorCode::DownloadFailed:
        case ErrorCode::ChecksumMismatch:
            error.code = code;
            break;
        default:
            error.code = ErrorCode::Unknown;
            break;
    }
    return error;
}

bool IsCancellationDetail(const std::string& detail) {
    return detail == "download cancelled by user";
}

bool IsSafeFileName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    // Windows trims trailing dots/spaces, so "evil " could collide with a
    // real file of the trimmed name; reject them (MAJOR-2 hardening).
    const char last = name.back();
    if (last == '.' || last == ' ') {
        return false;
    }
    // Reject ':' (drive root / alternate data streams), Windows-invalid
    // filename chars <>"|?*, and path separators.
    // SECURITY: catalog names are attacker-influenced via models.json (Q2).
    return name.find_first_of("/\\:<>\"|?*") == std::string::npos;
}

std::string ModelDisplayName(const ModelDownload& model) {
    return model.display_name.empty() ? model.name : model.display_name;
}

std::string LanguageDisplayName(const std::string& language) {
    if (language == "en") {
        return "English";
    }
    if (language == "hi-hinglish") {
        return "Hinglish (Hindi + English)";
    }
    if (language == "multi") {
        return "Multilingual";
    }
    return "Other";
}

std::string ModelSizeLabel(std::uint64_t size_bytes) {
    if (size_bytes == 0) {
        return "unknown size";
    }
    char buffer[32];
    if (size_bytes >= 1024ull * 1024ull * 1024ull) {
        std::snprintf(buffer, sizeof(buffer), "%.1f GB",
                      static_cast<double>(size_bytes) / (1024.0 * 1024.0 * 1024.0));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.0f MB",
                      static_cast<double>(size_bytes) / (1024.0 * 1024.0));
    }
    return buffer;
}

DownloadStartPlan PlanDownloadStart(std::uint64_t existing_bytes, std::uint64_t remote_size,
                                    const std::string& expected_sha256,
                                    const std::string& actual_sha256) {
    DownloadStartPlan start;
    const ResumePlan plan =
        PlanResume(existing_bytes, remote_size, expected_sha256, actual_sha256);
    if (!plan.valid) {
        return start;
    }
    start.resume_offset = plan.start_offset;
    // A complete file that needs no verification (matching hash, or no
    // expected hash) skips the transfer entirely; this mirrors the early
    // return the transfer layer makes on the same inputs.
    start.skip =
        existing_bytes > 0 && plan.start_offset == 0 && !plan.verify_required;
    return start;
}

bool DownloadModel::Start(const std::string& destination, std::uint64_t total) {
    if (snapshot_.state == DownloadState::Downloading || destination.empty()) {
        return false;
    }
    snapshot_.state = DownloadState::Downloading;
    snapshot_.downloaded = 0;
    snapshot_.total = total;
    snapshot_.destination = destination;
    snapshot_.error = DownloadError{};
    return true;
}

void DownloadModel::OnProgress(std::uint64_t downloaded) {
    if (snapshot_.state != DownloadState::Downloading) {
        return;
    }
    if (downloaded <= snapshot_.downloaded) {
        return;
    }
    if (snapshot_.total > 0 && downloaded > snapshot_.total) {
        downloaded = snapshot_.total;
    }
    snapshot_.downloaded = downloaded;
}

void DownloadModel::OnSuccess() {
    if (snapshot_.state != DownloadState::Downloading) {
        return;
    }
    snapshot_.state = DownloadState::Success;
}

void DownloadModel::OnCancelled() {
    if (snapshot_.state != DownloadState::Downloading) {
        return;
    }
    snapshot_.state = DownloadState::Cancelled;
}

void DownloadModel::OnFailed(const DownloadError& error) {
    if (snapshot_.state != DownloadState::Downloading) {
        return;
    }
    snapshot_.state = DownloadState::Failed;
    snapshot_.error = error;
}

void DownloadModel::Reset() {
    snapshot_ = DownloadSnapshot{};
}

DownloadSnapshot DownloadModel::Snapshot() const {
    return snapshot_;
}

bool DownloadModel::IsDownloading() const {
    return snapshot_.state == DownloadState::Downloading;
}

double DownloadModel::ProgressFraction() const {
    if (snapshot_.total == 0) {
        return -1.0;
    }
    const std::uint64_t clamped =
        snapshot_.downloaded > snapshot_.total ? snapshot_.total : snapshot_.downloaded;
    return static_cast<double>(clamped) / static_cast<double>(snapshot_.total);
}

}
