#include "stt/cloud/ElevenLabsResponse.h"

#include "stt/cloud/HttpStatus.h"

#include <nlohmann/json.hpp>

namespace {

const nlohmann::json* FindTextField(const nlohmann::json& json) {
    for (const char* key : {"text", "transcript"}) {
        if (json.contains(key) && json.at(key).is_string()) {
            return &json.at(key);
        }
    }
    return nullptr;
}

std::string ExtractErrorDetail(const nlohmann::json& json) {
    if (json.contains("detail")) {
        const nlohmann::json& detail = json.at("detail");
        if (detail.is_string()) {
            return detail.get<std::string>();
        }
        if (detail.is_object()) {
            std::string status;
            if (detail.contains("status") && detail.at("status").is_string()) {
                status = detail.at("status").get<std::string>();
            }
            std::string message;
            if (detail.contains("message") && detail.at("message").is_string()) {
                message = detail.at("message").get<std::string>();
            }
            if (!status.empty() && !message.empty()) {
                return status + ": " + message;
            }
            if (!message.empty()) {
                return message;
            }
            return status;
        }
    }
    for (const char* key : {"message", "error"}) {
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

bool ParseElevenLabsResponse(int http_status, const std::vector<std::uint8_t>& body,
                             const std::string& request_id_header, ElevenLabsSuccess& success,
                             ElevenLabsFailure& failure) {
    const nlohmann::json json =
        nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
    if (json.is_discarded()) {
        failure.category = http_status >= 400 ? ClassifyHttpStatus(http_status)
                                              : SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id_header;
        failure.detail = "unparseable response body";
        return false;
    }
    if (http_status < 200 || http_status >= 300) {
        failure.category = ClassifyHttpStatus(http_status);
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
    const nlohmann::json* text = FindTextField(json);
    if (text == nullptr) {
        failure.category = SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id;
        failure.detail = "no text field in success body";
        return false;
    }
    success.text = text->get<std::string>();
    if (json.contains("language_code") && json.at("language_code").is_string()) {
        success.language_code = json.at("language_code").get<std::string>();
    }
    success.request_id = request_id;
    return true;
}
