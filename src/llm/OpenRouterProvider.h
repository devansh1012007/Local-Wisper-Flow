#pragma once

#include "core/ILlmProvider.h"
#include "llm/OpenRouterContract.h"
#include "stt/cloud/IHttpTransport.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>

enum class OpenRouterError {
    None,
    Unknown,
    Auth,
    RateLimited,
    Server,
    Timeout,
    Network,
    InvalidRequest,
    Malformed,
    Cancelled,
    Disabled,
};

inline const char* ToString(OpenRouterError error) {
    switch (error) {
        case OpenRouterError::None:
            return "none";
        case OpenRouterError::Unknown:
            return "unknown";
        case OpenRouterError::Auth:
            return "auth";
        case OpenRouterError::RateLimited:
            return "rate_limited";
        case OpenRouterError::Server:
            return "server";
        case OpenRouterError::Timeout:
            return "timeout";
        case OpenRouterError::Network:
            return "network";
        case OpenRouterError::InvalidRequest:
            return "invalid_request";
        case OpenRouterError::Malformed:
            return "malformed";
        case OpenRouterError::Cancelled:
            return "cancelled";
        case OpenRouterError::Disabled:
            return "disabled";
    }
    return "unknown";
}

struct OpenRouterSettings {
    std::string api_key;
    std::string model;
    int timeout_ms = OpenRouterContract::kDefaultTimeoutMs;
};

struct OpenRouterOutcome {
    OpenRouterError error = OpenRouterError::None;
    int http_status = 0;
    std::string request_id;
    std::string detail;
    std::chrono::milliseconds latency{0};
    std::size_t bytes_sent = 0;
    std::size_t bytes_received = 0;
};

class OpenRouterProvider final : public ILlmProvider {
public:
    OpenRouterProvider(IHttpTransport& transport, OpenRouterSettings settings);

    OpenRouterProvider(const OpenRouterProvider&) = delete;
    OpenRouterProvider& operator=(const OpenRouterProvider&) = delete;

    std::string Process(const std::string& text, const std::string& system_prompt) override;
    void RequestCancel() override;
    OpenRouterOutcome LastOutcome() const;

private:
    HttpRequest BuildRequest(const std::string& text, const std::string& system_prompt) const;
    std::string ParseSuccessBody(const HttpResponse& response, OpenRouterOutcome& outcome);
    OpenRouterOutcome ClassifyFailure(const HttpResponse& response, const std::string& raw_detail) const;
    std::string RedactDetail(const std::string& raw) const;
    void StoreOutcome(const OpenRouterOutcome& outcome);
    void LogOutcome(const OpenRouterOutcome& outcome) const;

    IHttpTransport& transport_;
    OpenRouterSettings settings_;
    std::mutex mutex_;
    std::atomic<bool> cancel_requested_{false};
    mutable std::mutex outcome_mutex_;
    OpenRouterOutcome last_outcome_;
};
