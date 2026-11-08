#include "llm/OpenRouterModels.h"

#include "core/Redact.h"
#include "llm/OpenRouterContract.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace {

std::string LowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string RedactDetail(const std::string& raw, const std::string& apiKey) {
    std::string redacted = lwf::RedactSecret(lwf::RedactAuthValues(raw), apiKey);
    if (redacted.size() > OpenRouterContract::kMaxDetailChars) {
        redacted.resize(OpenRouterContract::kMaxDetailChars);
    }
    return redacted;
}

}  // namespace

namespace OpenRouterModels {

ModelList FetchOpenRouterModels(IHttpTransport& transport, const std::string& apiKey) {
    ModelList result;

    HttpRequest request;
    request.host = OpenRouterContract::kHost;
    request.path = OpenRouterContract::kModelsPath;
    if (!apiKey.empty()) {
        request.headers.emplace_back(OpenRouterContract::kApiKeyHeader, "Bearer " + apiKey);
    }
    request.content_type = "";
    request.body = {};
    request.timeout_ms = OpenRouterContract::kDefaultTimeoutMs;

    HttpResponse response;
    try {
        response = transport.Get(request);
    } catch (const HttpTransportError& error) {
        result.status = FetchStatus::Network;
        result.errorDetail = RedactDetail(error.what() != nullptr ? error.what() : "transport failed", apiKey);
        return result;
    } catch (const std::exception& error) {
        result.status = FetchStatus::Network;
        result.errorDetail = RedactDetail(error.what() != nullptr ? error.what() : "transport failed", apiKey);
        return result;
    }

    const std::string bodyText(response.body.begin(), response.body.end());

    if (response.status < 200 || response.status >= 300) {
        result.status = FetchStatus::NonOk;
        result.errorDetail = RedactDetail(bodyText, apiKey);
        return result;
    }

    const nlohmann::json parsed = nlohmann::json::parse(bodyText, nullptr, false);
    if (parsed.is_discarded()) {
        result.status = FetchStatus::BadJson;
        result.errorDetail = RedactDetail("unparseable response body", apiKey);
        return result;
    }
    if (!parsed.is_object() || !parsed.contains(OpenRouterContract::kDataField) ||
        !parsed.at(OpenRouterContract::kDataField).is_array()) {
        result.status = FetchStatus::BadJson;
        result.errorDetail = RedactDetail("missing or invalid data field in response body", apiKey);
        return result;
    }

    const nlohmann::json& data = parsed.at(OpenRouterContract::kDataField);
    std::vector<std::string> ids;
    for (const auto& entry : data) {
        if (!entry.is_object()) {
            continue;
        }
        if (!entry.contains(OpenRouterContract::kIdField) ||
            !entry.at(OpenRouterContract::kIdField).is_string()) {
            continue;
        }
        const std::string id = entry.at(OpenRouterContract::kIdField).get<std::string>();
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
            continue;
        }
        ids.push_back(id);
    }

    std::stable_sort(ids.begin(), ids.end(), [](const std::string& lhs, const std::string& rhs) {
        return LowerAscii(lhs) < LowerAscii(rhs);
    });

    if (ids.empty()) {
        result.status = FetchStatus::Empty;
        result.modelIds = {};
        result.errorDetail = "";
        return result;
    }

    result.status = FetchStatus::Ok;
    result.modelIds = std::move(ids);
    result.errorDetail = "";
    return result;
}

}  // namespace OpenRouterModels
