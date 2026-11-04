#include "stt/cloud/MuseResponse.h"

#include "stt/cloud/HttpStatus.h"

#include <nlohmann/json.hpp>

namespace {

std::string ExtractErrorDetail(const nlohmann::json& json) {
    if (json.contains("error")) {
        const nlohmann::json& error = json.at("error");
        if (error.is_string()) {
            return error.get<std::string>();
        }
        if (error.is_object() && error.contains("message") && error.at("message").is_string()) {
            return error.at("message").get<std::string>();
        }
    }
    for (const char* key : {"message", "detail"}) {
        if (json.contains(key) && json.at(key).is_string()) {
            return json.at(key).get<std::string>();
        }
    }
    return {};
}

std::string ExtractSessionId(const nlohmann::json& json) {
    if (json.contains("sessionId") && json.at("sessionId").is_string()) {
        return json.at("sessionId").get<std::string>();
    }
    return {};
}

}  // namespace

bool ParseMuseResponse(int http_status, const std::vector<std::uint8_t>& body,
                       MuseSuccess& success, MuseFailure& failure) {
    const nlohmann::json json =
        nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
    if (json.is_discarded()) {
        failure.category = http_status >= 400 ? ClassifyHttpStatus(http_status)
                                              : SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.detail = "unparseable response body";
        return false;
    }
    const bool success_status = http_status >= 200 && http_status < 300;
    if (json.is_object()) {
        if (success_status) {
            success.session_id = ExtractSessionId(json);
        } else {
            failure.http_status = http_status;
            failure.category = ClassifyHttpStatus(http_status);
            failure.session_id = ExtractSessionId(json);
            failure.detail = ExtractErrorDetail(json);
            return false;
        }
    } else {
        failure.http_status = http_status;
        failure.category = success_status ? SttErrorCategory::Unknown
                                          : ClassifyHttpStatus(http_status);
        failure.detail = success_status ? "success body is not a JSON object"
                                        : "unparseable response body";
        return false;
    }
    if (!json.contains("transcript") || !json.at("transcript").is_string()) {
        failure.http_status = http_status;
        failure.category = SttErrorCategory::Unknown;
        failure.session_id = success.session_id;
        failure.detail = "no transcript field in success body";
        return false;
    }
    success.transcript = json.at("transcript").get<std::string>();
    return true;
}
