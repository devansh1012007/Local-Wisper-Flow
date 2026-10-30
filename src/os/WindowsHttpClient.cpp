#include "os/WindowsHttpClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class WinHttpHandle {
public:
    WinHttpHandle() = default;
    ~WinHttpHandle() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    bool Valid() const {
        return handle_ != nullptr;
    }

    HINTERNET Get() const {
        return handle_;
    }

    void Reset(HINTERNET handle) {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HINTERNET handle_ = nullptr;
};

std::wstring ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0], size);
    return wide;
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0, nullptr,
                                         nullptr);
    if (size <= 0) {
        return std::string();
    }
    std::string narrow(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &narrow[0], size,
                        nullptr, nullptr);
    return narrow;
}

HttpTransportError MakeTransportError(const char* what) {
    const DWORD last_error = GetLastError();
    if (last_error == ERROR_WINHTTP_TIMEOUT || last_error == ERROR_WINHTTP_RESEND_REQUEST) {
        return HttpTransportError(HttpTransportErrorKind::Timeout, what);
    }
    return HttpTransportError(HttpTransportErrorKind::Network, what);
}

std::vector<std::pair<std::string, std::string>> ParseRawHeaders(const std::wstring& raw) {
    std::vector<std::pair<std::string, std::string>> headers;
    std::size_t position = 0;
    while (position < raw.size()) {
        const std::size_t line_end = raw.find(L'\n', position);
        std::wstring line = raw.substr(position, line_end == std::wstring::npos
                                                     ? std::wstring::npos
                                                     : line_end - position);
        position = line_end == std::wstring::npos ? raw.size() : line_end + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const std::size_t colon = line.find(L':');
        if (colon == std::wstring::npos) {
            continue;
        }
        std::wstring name = line.substr(0, colon);
        std::wstring value = line.substr(colon + 1);
        while (!value.empty() && value.front() == L' ') {
            value.erase(value.begin());
        }
        headers.emplace_back(ToUtf8(name), ToUtf8(value));
    }
    return headers;
}

}  // namespace

HttpResponse WindowsHttpClient::Post(const HttpRequest& request) {
    WinHttpHandle session;
    session.Reset(WinHttpOpen(L"LocalWisperFlow/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.Valid()) {
        throw MakeTransportError("WinHttpOpen failed");
    }
    WinHttpHandle connection;
    connection.Reset(WinHttpConnect(session.Get(), ToWide(request.host).c_str(),
                                    INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection.Valid()) {
        throw MakeTransportError("WinHttpConnect failed");
    }
    WinHttpHandle http;
    http.Reset(WinHttpOpenRequest(connection.Get(), L"POST", ToWide(request.path).c_str(), nullptr,
                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE));
    if (!http.Valid()) {
        throw MakeTransportError("WinHttpOpenRequest failed");
    }

    const int timeout = request.timeout_ms > 0 ? request.timeout_ms : 15000;
    WinHttpSetTimeouts(http.Get(), timeout, timeout, timeout, timeout);

    std::wstring headers;
    for (const auto& header : request.headers) {
        headers += ToWide(header.first) + L": " + ToWide(header.second) + L"\r\n";
    }
    headers += L"Content-Type: " + ToWide(request.content_type) + L"\r\n";

    if (!WinHttpSendRequest(http.Get(), headers.c_str(), static_cast<DWORD>(-1),
                            const_cast<BYTE*>(request.body.data()),
                            static_cast<DWORD>(request.body.size()),
                            static_cast<DWORD>(request.body.size()), 0)) {
        throw MakeTransportError("WinHttpSendRequest failed");
    }
    if (!WinHttpReceiveResponse(http.Get(), nullptr)) {
        throw MakeTransportError("WinHttpReceiveResponse failed");
    }

    HttpResponse response;
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(http.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX)) {
        throw MakeTransportError("WinHttpQueryHeaders status failed");
    }
    response.status = static_cast<int>(status);

    DWORD raw_size = 0;
    if (WinHttpQueryHeaders(http.Get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                            WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &raw_size,
                            WINHTTP_NO_HEADER_INDEX) ||
        GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::wstring raw(static_cast<std::size_t>(raw_size) / sizeof(wchar_t), L'\0');
        if (raw_size > 0 &&
            WinHttpQueryHeaders(http.Get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, &raw[0], &raw_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            raw.resize(static_cast<std::size_t>(raw_size) / sizeof(wchar_t));
            response.headers = ParseRawHeaders(raw);
        }
    }

    DWORD available = 0;
    while (WinHttpQueryDataAvailable(http.Get(), &available) && available > 0) {
        std::vector<BYTE> chunk(static_cast<std::size_t>(available));
        DWORD read = 0;
        if (!WinHttpReadData(http.Get(), chunk.data(), available, &read)) {
            throw MakeTransportError("WinHttpReadData failed");
        }
        chunk.resize(static_cast<std::size_t>(read));
        response.body.insert(response.body.end(), chunk.begin(), chunk.end());
    }
    return response;
}
