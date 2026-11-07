#include "llm/OpenRouterProvider.h"

#include "core/Log.h"
#include "core/Redact.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace {

std::string LowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string StringField(const nlohmann::json& value, const char* key) {
    if (!value.is_object()) {
        return {};
    }
    if (!value.contains(key)) {
        return {};
    }
    const nlohmann::json& field = value.at(key);
    if (!field.is_string()) {
        return {};
    }
    return field.get<std::string>();
}

OpenRouterError ErrorTypeToCategory(const std::string& error_type) {
    const std::string key = LowerAscii(error_type);
    if (key == "authentication" || key == "payment_required" || key == "permission_denied" ||
        key == "content_policy_violation" || key == "refusal") {
        return OpenRouterError::Auth;
    }
    if (key == "rate_limit_exceeded") {
        return OpenRouterError::RateLimited;
    }
    if (key == "timeout") {
        return OpenRouterError::Timeout;
    }
    if (key == "server" || key == "unmapped" || key == "provider_overloaded" ||
        key == "provider_unavailable") {
        return OpenRouterError::Server;
    }
    if (key == "invalid_request" || key == "invalid_prompt" ||
        key == "context_length_exceeded" || key == "max_tokens_exceeded" ||
        key == "token_limit_exceeded" || key == "string_too_long" ||
        key == "payload_too_large" || key == "unprocessable" || key == "not_found" ||
        key == "precondition_failed" || key == "invalid_image" || key == "image_too_large" ||
        key == "image_too_small" || key == "unsupported_image_format" ||
        key == "image_not_found" || key == "image_download_failed") {
        return OpenRouterError::InvalidRequest;
    }
    return OpenRouterError::Unknown;
}

OpenRouterError StatusToCategory(int status) {
    if (status == 401 || status == 403 || status == 402) {
        return OpenRouterError::Auth;
    }
    if (status == 408) {
        return OpenRouterError::Timeout;
    }
    if (status == 429) {
        return OpenRouterError::RateLimited;
    }
    if (status == 413) {
        return OpenRouterError::InvalidRequest;
    }
    if (status >= 500) {
        return OpenRouterError::Server;
    }
    if (status >= 400 && status < 500) {
        return OpenRouterError::InvalidRequest;
    }
    if (status == 0) {
        return OpenRouterError::Network;
    }
    return OpenRouterError::Unknown;
}

std::string ExtractErrorDetail(const nlohmann::json& json) {
    if (json.is_object() && json.contains(OpenRouterContract::kErrorField)) {
        const nlohmann::json& error = json.at(OpenRouterContract::kErrorField);
        if (error.is_string()) {
            return error.get<std::string>();
        }
        if (error.is_object()) {
            const std::string message = StringField(error, OpenRouterContract::kErrorMessageField);
            if (!message.empty()) {
                return message;
            }
        }
        if (error.is_object() && error.contains(OpenRouterContract::kCodeField)) {
            const nlohmann::json& code = error.at(OpenRouterContract::kCodeField);
            if (code.is_string()) {
                return code.get<std::string>();
            }
        }
    }
    const std::string top = StringField(json, OpenRouterContract::kErrorMessageField);
    if (!top.empty()) {
        return top;
    }
    return StringField(json, OpenRouterContract::kDetailField);
}

std::string ExtractErrorType(const nlohmann::json& json) {
    if (!json.is_object() || !json.contains(OpenRouterContract::kErrorField)) {
        return {};
    }
    const nlohmann::json& error = json.at(OpenRouterContract::kErrorField);
    if (!error.is_object() || !error.contains(OpenRouterContract::kMetadataField)) {
        return {};
    }
    const nlohmann::json& metadata = error.at(OpenRouterContract::kMetadataField);
    return StringField(metadata, OpenRouterContract::kErrorTypeField);
}

}  // namespace

OpenRouterProvider::OpenRouterProvider(IHttpTransport& transport, OpenRouterSettings settings)
    : transport_(transport), settings_(std::move(settings)) {
    if (settings_.api_key.empty()) {
        throw std::runtime_error("OpenRouterProvider: api key is required");
    }
    if (settings_.model.empty()) {
        throw std::runtime_error("OpenRouterProvider: model is required");
    }
}

void OpenRouterProvider::RequestCancel() {
    cancel_requested_.store(true);
}

OpenRouterOutcome OpenRouterProvider::LastOutcome() const {
    std::lock_guard<std::mutex> lock(outcome_mutex_);
    return last_outcome_;
}

HttpRequest OpenRouterProvider::BuildRequest(const std::string& text,
                                             const std::string& system_prompt) const {
    nlohmann::json body;
    body[OpenRouterContract::kModelField] = settings_.model;
    nlohmann::json messages = nlohmann::json::array();
    if (!system_prompt.empty()) {
        nlohmann::json system;
        system[OpenRouterContract::kRoleField] = OpenRouterContract::kSystemRole;
        system[OpenRouterContract::kContentField] = system_prompt;
        messages.push_back(std::move(system));
    }
    nlohmann::json user;
    user[OpenRouterContract::kRoleField] = OpenRouterContract::kUserRole;
    user[OpenRouterContract::kContentField] = text;
    messages.push_back(std::move(user));
    body[OpenRouterContract::kMessagesField] = std::move(messages);
    const std::string serialized = body.dump();
    HttpRequest request;
    request.host = OpenRouterContract::kHost;
    request.path = OpenRouterContract::kPath;
    request.headers.emplace_back(OpenRouterContract::kApiKeyHeader, "Bearer " + settings_.api_key);
    request.content_type = OpenRouterContract::kContentType;
    request.body.assign(serialized.begin(), serialized.end());
    request.timeout_ms = settings_.timeout_ms > 0 ? settings_.timeout_ms
                                                     : OpenRouterContract::kDefaultTimeoutMs;
    return request;
}

std::string OpenRouterProvider::RedactDetail(const std::string& raw) const {
    std::string redacted = lwf::RedactSecret(lwf::RedactAuthValues(raw), settings_.api_key);
    if (redacted.size() > OpenRouterContract::kMaxDetailChars) {
        redacted.resize(OpenRouterContract::kMaxDetailChars);
    }
    return redacted;
}

void OpenRouterProvider::StoreOutcome(const OpenRouterOutcome& outcome) {
    std::lock_guard<std::mutex> lock(outcome_mutex_);
    last_outcome_ = outcome;
}

void OpenRouterProvider::LogOutcome(const OpenRouterOutcome& outcome) const {
    if (outcome.error == OpenRouterError::None) {
        return;
    }
    lwf::Log::Warning(std::string("llm provider=") + OpenRouterContract::kApiKeyProvider +
                      " status=" + std::to_string(outcome.http_status) +
                      " error=" + ToString(outcome.error) +
                      " latency_ms=" + std::to_string(outcome.latency.count()) +
                      " detail=" + outcome.detail);
}

OpenRouterOutcome OpenRouterProvider::ClassifyFailure(const HttpResponse& response,
                                                      const std::string& raw_detail) const {
    const nlohmann::json json =
        nlohmann::json::parse(response.body.begin(), response.body.end(), nullptr, false);
    OpenRouterOutcome outcome;
    outcome.http_status = response.status;
    outcome.bytes_received = response.body.size();
    std::string type;
    std::string detail = raw_detail;
    if (!json.is_discarded() && json.is_object()) {
        if (json.contains(OpenRouterContract::kIdField) &&
            json.at(OpenRouterContract::kIdField).is_string()) {
            outcome.request_id = json.at(OpenRouterContract::kIdField).get<std::string>();
        }
        if (json.contains(OpenRouterContract::kErrorField)) {
            type = ExtractErrorType(json);
            const std::string vendor = ExtractErrorDetail(json);
            if (!vendor.empty()) {
                detail = vendor;
            }
        } else {
            const std::string vendor = ExtractErrorDetail(json);
            if (!vendor.empty()) {
                detail = vendor;
            }
        }
    }
    const OpenRouterError typed = ErrorTypeToCategory(type);
    outcome.error = typed != OpenRouterError::Unknown ? typed : StatusToCategory(response.status);
    outcome.detail = RedactDetail(detail.empty() ? "request failed" : detail);
    return outcome;
}

std::string OpenRouterProvider::ParseSuccessBody(const HttpResponse& response,
                                                 OpenRouterOutcome& outcome) {
    const nlohmann::json json =
        nlohmann::json::parse(response.body.begin(), response.body.end(), nullptr, false);
    outcome.http_status = response.status;
    outcome.bytes_received = response.body.size();
    if (json.is_discarded()) {
        outcome.error = OpenRouterError::Malformed;
        outcome.detail = RedactDetail("unparseable response body");
        return {};
    }
    if (!json.is_object()) {
        outcome.error = OpenRouterError::Malformed;
        outcome.detail = RedactDetail("success body is not a JSON object");
        return {};
    }
    if (json.contains(OpenRouterContract::kIdField) &&
        json.at(OpenRouterContract::kIdField).is_string()) {
        outcome.request_id = json.at(OpenRouterContract::kIdField).get<std::string>();
    }
    if (json.contains(OpenRouterContract::kErrorField) &&
        !json.at(OpenRouterContract::kErrorField).is_null()) {
        const OpenRouterOutcome failure = ClassifyFailure(response, ExtractErrorDetail(json));
        outcome.error = failure.error;
        outcome.detail = failure.detail;
        outcome.request_id = failure.request_id;
        return {};
    }
    if (!json.contains(OpenRouterContract::kChoicesField) ||
        !json.at(OpenRouterContract::kChoicesField).is_array() ||
        json.at(OpenRouterContract::kChoicesField).empty()) {
        outcome.error = OpenRouterError::Malformed;
        outcome.detail = RedactDetail("no choices field in success body");
        return {};
    }
    const nlohmann::json& choice = json.at(OpenRouterContract::kChoicesField).at(0);
    if (!choice.is_object()) {
        outcome.error = OpenRouterError::Malformed;
        outcome.detail = RedactDetail("choice is not a JSON object");
        return {};
    }
    if (choice.contains(OpenRouterContract::kMessageField) &&
        choice.at(OpenRouterContract::kMessageField).is_object()) {
        const nlohmann::json& message = choice.at(OpenRouterContract::kMessageField);
        if (message.contains(OpenRouterContract::kContentField) &&
            message.at(OpenRouterContract::kContentField).is_string()) {
            outcome.error = OpenRouterError::None;
            return message.at(OpenRouterContract::kContentField).get<std::string>();
        }
        if (message.contains(OpenRouterContract::kContentField) &&
            message.at(OpenRouterContract::kContentField).is_null()) {
            outcome.error = OpenRouterError::Malformed;
            outcome.detail = RedactDetail("empty content in success body");
            return {};
        }
    }
    if (choice.contains(OpenRouterContract::kTextField) &&
        choice.at(OpenRouterContract::kTextField).is_string()) {
        outcome.error = OpenRouterError::None;
        return choice.at(OpenRouterContract::kTextField).get<std::string>();
    }
    outcome.error = OpenRouterError::Malformed;
    outcome.detail = RedactDetail("no content field in success body");
    return {};
}

std::string OpenRouterProvider::Process(const std::string& text, const std::string& system_prompt) {
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_requested_.store(false);
    if (text.empty()) {
        OpenRouterOutcome outcome;
        outcome.error = OpenRouterError::None;
        StoreOutcome(outcome);
        return {};
    }
    if (cancel_requested_.load()) {
        OpenRouterOutcome outcome;
        outcome.error = OpenRouterError::Cancelled;
        outcome.detail = RedactDetail("cancelled");
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    }
    HttpRequest request = BuildRequest(text, system_prompt);
    OpenRouterOutcome outcome;
    outcome.bytes_sent = request.body.size();
    const auto started = std::chrono::steady_clock::now();
    HttpResponse response;
    try {
        response = transport_.Post(request);
    } catch (const HttpTransportError& error) {
        outcome.error = error.Kind() == HttpTransportErrorKind::Timeout ? OpenRouterError::Timeout
                                                                         : OpenRouterError::Network;
        outcome.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        outcome.detail = RedactDetail(error.what() != nullptr ? error.what() : "transport failed");
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    } catch (const std::exception& error) {
        outcome.error = OpenRouterError::Network;
        outcome.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        outcome.detail = RedactDetail(error.what() != nullptr ? error.what() : "transport failed");
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    } catch (...) {
        outcome.error = OpenRouterError::Network;
        outcome.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        outcome.detail = RedactDetail("unknown transport failure");
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    }
    outcome.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    if (cancel_requested_.load()) {
        outcome.http_status = response.status;
        outcome.bytes_received = response.body.size();
        outcome.error = OpenRouterError::Cancelled;
        outcome.detail = RedactDetail("cancelled");
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    }
    if (response.status < 200 || response.status >= 300) {
        outcome = ClassifyFailure(response, ExtractErrorDetail(nlohmann::json::parse(
                                response.body.begin(), response.body.end(), nullptr, false)));
        outcome.bytes_sent = request.body.size();
        outcome.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        StoreOutcome(outcome);
        LogOutcome(outcome);
        return {};
    }
    OpenRouterOutcome parsed;
    parsed.bytes_sent = request.body.size();
    parsed.latency = outcome.latency;
    const std::string content = ParseSuccessBody(response, parsed);
    StoreOutcome(parsed);
    LogOutcome(parsed);
    if (parsed.error != OpenRouterError::None) {
        return {};
    }
    return content;
}
