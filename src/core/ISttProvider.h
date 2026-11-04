#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// Error categories shared by every cloud STT provider and the router.
// ClassifyHttpStatus() (stt/cloud/HttpStatus.h) maps HTTP statuses onto
// these; transport exceptions map onto Timeout/Network at the call site.
enum class SttErrorCategory {
    None,
    Unknown,
    Cancelled,
    Timeout,
    Network,
    Auth,
    RateLimited,
    PayloadTooLarge,
    Server,
    InvalidRequest,
};

inline const char* ToString(SttErrorCategory category) {
    switch (category) {
        case SttErrorCategory::None:
            return "none";
        case SttErrorCategory::Cancelled:
            return "cancelled";
        case SttErrorCategory::Timeout:
            return "timeout";
        case SttErrorCategory::Network:
            return "network";
        case SttErrorCategory::Auth:
            return "auth";
        case SttErrorCategory::RateLimited:
            return "rate_limited";
        case SttErrorCategory::PayloadTooLarge:
            return "payload_too_large";
        case SttErrorCategory::Server:
            return "server";
        case SttErrorCategory::InvalidRequest:
            return "invalid_request";
        case SttErrorCategory::Unknown:
            break;
    }
    return "unknown";
}

// One wire attempt against one provider. Stored on the transcription result
// so the router and the benchmark can report latency, bytes and request ids;
// `detail` must already be redacted by the producer (core/Redact.h).
struct SttAttemptStats {
    std::string provider;
    std::string request_id;
    int http_status = 0;
    SttErrorCategory error = SttErrorCategory::None;
    std::size_t bytes_sent = 0;
    std::size_t bytes_received = 0;
    std::chrono::milliseconds latency{0};
    std::string detail;
};

// Result of one cloud transcription. On failure `text` is empty, `success`
// is false and `error` carries the worst category seen across attempts.
struct SttTranscription {
    bool success = false;
    bool cancelled = false;
    std::string text;
    SttErrorCategory error = SttErrorCategory::None;
    std::vector<SttAttemptStats> attempts;
};

// Polled between clips; true means stop and report Cancelled.
using SttCancelCheck = std::function<bool()>;

// A cloud speech-to-text provider. Implementations own chunking (clip split,
// WAV encode, multipart build) behind Transcribe and never throw for wire
// failures - they return them on the result. Transport construction errors
// (missing api key) still throw std::runtime_error.
class ISttProvider {
public:
    virtual ~ISttProvider() = default;

    virtual SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                        const SttCancelCheck& cancel_check) = 0;
};
