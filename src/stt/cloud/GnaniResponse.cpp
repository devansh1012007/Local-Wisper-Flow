#include "stt/cloud/GnaniResponse.h"

#include <nlohmann/json.hpp>

namespace {

SttErrorCategory ClassifyStatus(int status) {
    if (status == 401 || status == 403) {
        return SttErrorCategory::Auth;
    }
    if (status == 429) {
        return SttErrorCategory::RateLimited;
    }
    if (status == 408) {
        return SttErrorCategory::Timeout;
    }
    if (status == 413) {
        return SttErrorCategory::PayloadTooLarge;
    }
    if (status >= 500) {
        return SttErrorCategory::Server;
    }
    if (status >= 400) {
        return SttErrorCategory::InvalidRequest;
    }
    return SttErrorCategory::Unknown;
}

const nlohmann::json* FindTranscriptField(const nlohmann::json& json) {
    for (const char* key : {"transcript", "text"}) {
        if (json.contains(key) && json.at(key).is_string()) {
            return &json.at(key);
        }
    }
    if (json.contains("data") && json.at("data").is_object()) {
        for (const char* key : {"transcript", "text"}) {
            if (json.at("data").contains(key) && json.at("data").at(key).is_string()) {
                return &json.at("data").at(key);
            }
        }
    }
    return nullptr;
}

std::string ExtractErrorDetail(const nlohmann::json& json) {
    if (json.contains("error")) {
        const nlohmann::json& error = json.at("error");
        if (error.is_object()) {
            std::string code;
            if (error.contains("code") && error.at("code").is_string()) {
                code = error.at("code").get<std::string>();
            } else if (error.contains("type") && error.at("type").is_string()) {
                code = error.at("type").get<std::string>();
            }
            std::string message;
            if (error.contains("message") && error.at("message").is_string()) {
                message = error.at("message").get<std::string>();
            }
            if (!code.empty() && !message.empty()) {
                return code + ": " + message;
            }
            if (!code.empty()) {
                return code;
            }
            return message;
        }
        if (error.is_string()) {
            return error.get<std::string>();
        }
    }
    for (const char* key : {"message", "detail"}) {
        if (json.contains(key) && json.at(key).is_string()) {
            return json.at(key).get<std::string>();
        }
    }
    return {};
}

std::string ExtractRequestId(const nlohmann::json& json) {
    for (const char* key : {"request_id", "id"}) {
        if (json.contains(key) && json.at(key).is_string()) {
            return json.at(key).get<std::string>();
        }
    }
    return {};
}

}  // namespace

bool ParseGnaniResponse(int http_status, const std::vector<std::uint8_t>& body,
                        const std::string& request_id_header, GnaniSuccess& success,
                        GnaniFailure& failure) {
    const nlohmann::json json =
        nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
    if (json.is_discarded()) {
        failure.category = http_status >= 400 ? ClassifyStatus(http_status)
                                              : SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id_header;
        failure.detail = "unparseable response body";
        return false;
    }
    if (http_status < 200 || http_status >= 300) {
        failure.category = ClassifyStatus(http_status);
        failure.http_status = http_status;
        failure.request_id = request_id_header;
        if (failure.request_id.empty() && json.is_object()) {
            failure.request_id = ExtractRequestId(json);
        }
        failure.detail = json.is_object() ? ExtractErrorDetail(json) : std::string();
        return false;
    }
    if (!json.is_object()) {
        failure.category = SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id_header;
        failure.detail = "success body is not a JSON object";
        return false;
    }
    std::string request_id = request_id_header;
    if (request_id.empty()) {
        request_id = ExtractRequestId(json);
    }
    const nlohmann::json* transcript = FindTranscriptField(json);
    if (transcript == nullptr) {
        failure.category = SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id;
        failure.detail = "no transcript field in success body";
        return false;
    }
    success.transcript = transcript->get<std::string>();
    success.request_id = request_id;
    return true;
}
