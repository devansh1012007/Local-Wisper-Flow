#include "stt/cloud/GnaniResponse.h"

#include "stt/cloud/HttpStatus.h"

#include <nlohmann/json.hpp>

namespace {

std::string ExtractErrorDetail(const nlohmann::json& json) {
    if (json.contains("error")) {
        const nlohmann::json& error = json.at("error");
        if (error.is_string()) {
            return error.get<std::string>();
        }
        if (error.is_object()) {
            std::string code;
            if (error.contains("type") && error.at("type").is_string()) {
                code = error.at("type").get<std::string>();
            } else if (error.contains("status") && error.at("status").is_string()) {
                code = error.at("status").get<std::string>();
            }
            std::string message;
            if (error.contains("message") && error.at("message").is_string()) {
                message = error.at("message").get<std::string>();
            }
            if (!code.empty() && !message.empty()) {
                return code + ": " + message;
            }
            if (!message.empty()) {
                return message;
            }
            return code;
        }
    }
    if (json.contains("detail")) {
        const nlohmann::json& detail = json.at("detail");
        if (detail.is_string()) {
            return detail.get<std::string>();
        }
        if (detail.is_object()) {
            std::string code;
            if (detail.contains("error_code") && detail.at("error_code").is_string()) {
                code = detail.at("error_code").get<std::string>();
            }
            std::string message;
            if (detail.contains("message") && detail.at("message").is_string()) {
                message = detail.at("message").get<std::string>();
            }
            if (!code.empty() && !message.empty()) {
                return code + ": " + message;
            }
            if (!message.empty()) {
                return message;
            }
            return code;
        }
        if (detail.is_array() && !detail.empty()) {
            const nlohmann::json& first = detail.at(0);
            if (first.is_object()) {
                std::string type;
                if (first.contains("type") && first.at("type").is_string()) {
                    type = first.at("type").get<std::string>();
                }
                std::string message;
                if (first.contains("msg") && first.at("msg").is_string()) {
                    message = first.at("msg").get<std::string>();
                }
                if (!type.empty() && !message.empty()) {
                    return type + ": " + message;
                }
                if (!message.empty()) {
                    return message;
                }
                return type;
            }
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
    if (!json.contains("transcript") || !json.at("transcript").is_string()) {
        failure.category = SttErrorCategory::Unknown;
        failure.http_status = http_status;
        failure.request_id = request_id;
        failure.detail = "no transcript field in success body";
        return false;
    }
    success.transcript = json.at("transcript").get<std::string>();
    success.request_id = request_id;
    return true;
}
