#include "stt/cloud/WindowsHttpClient.h"

#include <windows.h>
#include <winhttp.h>

#include <stdexcept>
#include <utility>

namespace {

std::string LastMessage(const char* context) {
    return std::string(context) + " failed (WinHttp error " +
           std::to_string(GetLastError()) + ")";
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                         static_cast<int>(wide.size()), nullptr, 0, nullptr,
                                         nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(),
                        size, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(),
                        size);
    return out;
}

std::vector<std::pair<std::string, std::string>> ParseResponseHeaders(const std::wstring& raw) {
    std::vector<std::pair<std::string, std::string>> headers;
    std::size_t pos = 0;
    while (pos < raw.size()) {
        std::size_t stop = raw.find(L"\r\n", pos);
        stop = stop == std::wstring::npos ? raw.size() : stop;
        const std::wstring line = raw.substr(pos, stop - pos);
        pos = stop + 2;
        const std::size_t colon = line.find(L':');
        if (colon == std::wstring::npos || colon == 0) {
            continue;  // status line and malformed lines
        }
        std::size_t value_begin = colon + 1;
        while (value_begin < line.size() && line[value_begin] == L' ') {
            ++value_begin;
        }
        headers.emplace_back(WideToUtf8(line.substr(0, colon)),
                             WideToUtf8(line.substr(value_begin)));
    }
    return headers;
}

}  // namespace

WindowsHttpClient::~WindowsHttpClient() = default;

HttpResponse WindowsHttpClient::Post(const HttpRequest& request) {
    const int timeout_ms = request.timeout_ms > 0 ? request.timeout_ms : 15000;
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET handle = nullptr;

    auto fail = [&](HttpTransportErrorKind kind, const char* context) {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
        if (connection != nullptr) {
            WinHttpCloseHandle(connection);
        }
        if (session != nullptr) {
            WinHttpCloseHandle(session);
        }
        throw HttpTransportError(kind, LastMessage(context));
    };

    session = WinHttpOpen(L"LocalWisperFlow", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpOpen");
    }
    connection = WinHttpConnect(session, Utf8ToWide(request.host).c_str(),
                                INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpConnect");
    }
    handle = WinHttpOpenRequest(connection, L"POST", Utf8ToWide(request.path).c_str(), nullptr,
                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                WINHTTP_FLAG_SECURE);
    if (handle == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpOpenRequest");
    }

    const int ms = timeout_ms;
    WinHttpSetTimeouts(handle, ms, ms, ms, ms);

    std::wstring extra_headers;
    for (const auto& header : request.headers) {
        extra_headers += Utf8ToWide(header.first) + L": " + Utf8ToWide(header.second) + L"\r\n";
    }
    if (!request.content_type.empty()) {
        extra_headers += L"Content-Type: " + Utf8ToWide(request.content_type) + L"\r\n";
    }

    const DWORD total = static_cast<DWORD>(request.body.size());
    if (!WinHttpSendRequest(handle, extra_headers.c_str(),
                            static_cast<DWORD>(extra_headers.size()),
                            request.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                 : const_cast<LPBYTE>(request.body.data()),
                            total, total, 0)) {
        fail(HttpTransportErrorKind::Network, "WinHttpSendRequest");
    }
    if (!WinHttpReceiveResponse(handle, 0)) {
        fail(HttpTransportErrorKind::Network, "WinHttpReceiveResponse");
    }

    HttpResponse response;
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                        WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status_code);

    DWORD raw_size = 0;
    WinHttpQueryHeaders(handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                        WINHTTP_NO_OUTPUT_BUFFER, &raw_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::wstring raw(static_cast<std::size_t>(raw_size / sizeof(wchar_t)), L'\0');
        if (WinHttpQueryHeaders(handle, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &raw_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            response.headers = ParseResponseHeaders(raw);
        }
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(handle, &available)) {
            fail(HttpTransportErrorKind::Network, "WinHttpQueryDataAvailable");
        }
        if (available == 0) {
            break;
        }
        std::vector<std::uint8_t> chunk(available, 0);
        DWORD read = 0;
        if (!WinHttpReadData(handle, chunk.data(), available, &read)) {
            fail(HttpTransportErrorKind::Network, "WinHttpReadData");
        }
        chunk.resize(read);
        response.body.insert(response.body.end(), chunk.begin(), chunk.end());
    }

    WinHttpCloseHandle(handle);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response;
}

HttpResponse WindowsHttpClient::Get(const HttpRequest& request) {
    const int timeout_ms = request.timeout_ms > 0 ? request.timeout_ms : 15000;
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET handle = nullptr;

    auto fail = [&](HttpTransportErrorKind kind, const char* context) {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
        if (connection != nullptr) {
            WinHttpCloseHandle(connection);
        }
        if (session != nullptr) {
            WinHttpCloseHandle(session);
        }
        throw HttpTransportError(kind, LastMessage(context));
    };

    session = WinHttpOpen(L"LocalWisperFlow", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpOpen");
    }
    connection = WinHttpConnect(session, Utf8ToWide(request.host).c_str(),
                                INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpConnect");
    }
    handle = WinHttpOpenRequest(connection, L"GET", Utf8ToWide(request.path).c_str(), nullptr,
                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                WINHTTP_FLAG_SECURE);
    if (handle == nullptr) {
        fail(HttpTransportErrorKind::Network, "WinHttpOpenRequest");
    }

    const int ms = timeout_ms;
    WinHttpSetTimeouts(handle, ms, ms, ms, ms);

    std::wstring extra_headers;
    for (const auto& header : request.headers) {
        extra_headers += Utf8ToWide(header.first) + L": " + Utf8ToWide(header.second) + L"\r\n";
    }

    if (!WinHttpSendRequest(handle, extra_headers.c_str(),
                            static_cast<DWORD>(extra_headers.size()),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        fail(HttpTransportErrorKind::Network, "WinHttpSendRequest");
    }
    if (!WinHttpReceiveResponse(handle, 0)) {
        fail(HttpTransportErrorKind::Network, "WinHttpReceiveResponse");
    }

    HttpResponse response;
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                        WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status_code);

    DWORD raw_size = 0;
    WinHttpQueryHeaders(handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                        WINHTTP_NO_OUTPUT_BUFFER, &raw_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::wstring raw(static_cast<std::size_t>(raw_size / sizeof(wchar_t)), L'\0');
        if (WinHttpQueryHeaders(handle, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &raw_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            response.headers = ParseResponseHeaders(raw);
        }
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(handle, &available)) {
            fail(HttpTransportErrorKind::Network, "WinHttpQueryDataAvailable");
        }
        if (available == 0) {
            break;
        }
        std::vector<std::uint8_t> chunk(available, 0);
        DWORD read = 0;
        if (!WinHttpReadData(handle, chunk.data(), available, &read)) {
            fail(HttpTransportErrorKind::Network, "WinHttpReadData");
        }
        chunk.resize(read);
        response.body.insert(response.body.end(), chunk.begin(), chunk.end());
    }

    WinHttpCloseHandle(handle);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response;
}
