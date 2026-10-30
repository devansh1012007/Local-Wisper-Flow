#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct HttpRequest {
    std::string host;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string content_type;
    std::vector<std::uint8_t> body;
    int timeout_ms = 15000;
};

struct HttpResponse {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::uint8_t> body;

    std::string Header(const std::string& name) const {
        for (const auto& header : headers) {
            if (header.first.size() == name.size()) {
                bool equal = true;
                for (std::size_t i = 0; i < name.size(); ++i) {
                    const char left = static_cast<char>(header.first[i] | 0x20);
                    const char right = static_cast<char>(name[i] | 0x20);
                    if (left != right) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    return header.second;
                }
            }
        }
        return {};
    }
};

enum class HttpTransportErrorKind {
    Timeout,
    Network,
};

class HttpTransportError : public std::runtime_error {
public:
    HttpTransportError(HttpTransportErrorKind kind, const std::string& what)
        : std::runtime_error(what), kind_(kind) {}

    HttpTransportErrorKind Kind() const {
        return kind_;
    }

private:
    HttpTransportErrorKind kind_;
};

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual HttpResponse Post(const HttpRequest& request) = 0;
};
