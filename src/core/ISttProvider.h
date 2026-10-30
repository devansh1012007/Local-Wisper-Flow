#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

enum class SttErrorCategory {
    None,
    Auth,
    RateLimited,
    Timeout,
    Network,
    InvalidRequest,
    PayloadTooLarge,
    Server,
    Cancelled,
    Unknown,
};

struct SttAttemptStats {
    std::string provider;
    std::string request_id;
    int http_status = 0;
    SttErrorCategory error = SttErrorCategory::None;
    std::size_t bytes_sent = 0;
    std::size_t bytes_received = 0;
    std::chrono::milliseconds latency{0};
};

struct SttTranscription {
    std::string text;
    bool success = false;
    bool cancelled = false;
    SttErrorCategory error = SttErrorCategory::None;
    std::vector<SttAttemptStats> attempts;
};

using SttCancelCheck = std::function<bool()>;

class ISttProvider {
public:
    virtual ~ISttProvider() = default;

    virtual SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                        const SttCancelCheck& cancel_check) = 0;
};
