#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Transport-level request. Providers build it via the per-vendor
// Build*Request() helpers; only timeout_ms is tuned afterwards.
struct HttpRequest {
    std::string host;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string content_type;
    std::vector<std::uint8_t> body;
    int timeout_ms = 0;
};

struct HttpResponse {
    int status = 0;
    std::vector<std::uint8_t> body;
    std::vector<std::pair<std::string, std::string>> headers;

    // Case-insensitive header lookup; empty string when absent.
    std::string Header(const char* name) const {
        std::string lower;
        for (const char* p = name; p != nullptr && *p != '\0'; ++p) {
            const char c = *p;
            lower.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
        }
        for (const auto& header : headers) {
            std::string key;
            key.reserve(header.first.size());
            for (const char c : header.first) {
                key.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
            }
            if (key == lower) {
                return header.second;
            }
        }
        return {};
    }
};

enum class HttpTransportErrorKind { Timeout, Network };

class HttpTransportError : public std::exception {
public:
    HttpTransportError(HttpTransportErrorKind kind, std::string message)
        : kind_(kind), message_(std::move(message)) {}

    HttpTransportErrorKind Kind() const { return kind_; }

    const char* what() const noexcept override { return message_.c_str(); }

private:
    HttpTransportErrorKind kind_;
    std::string message_;
};

// The only network seam of the cloud STT stack. Production injects the
// Windows HTTP client (see WindowsHttpClient.h); tests and the benchmark
// inject fakes, which is what keeps the no-network gate honest on this box.
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual HttpResponse Post(const HttpRequest& request) = 0;
};
