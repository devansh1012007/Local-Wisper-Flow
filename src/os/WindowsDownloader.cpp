#include "os/WindowsDownloader.h"

#include "core/ErrorCode.h"
#include "core/InjectionException.h"
#include "core/Log.h"
#include "core/Onboarding.h"
#include "core/Sha256.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace Onboarding {

namespace {

std::wstring Utf8ToWide(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0], size);
    return wide;
}

struct UrlParts {
    std::wstring host;
    std::wstring path;
    bool https = true;
};

UrlParts SplitUrl(const std::string& url) {
    UrlParts parts;
    const std::wstring wide = Utf8ToWide(url);
    std::wstring rest = wide;
    if (rest.rfind(L"https://", 0) == 0) {
        rest = rest.substr(8);
        parts.https = true;
    } else if (rest.rfind(L"http://", 0) == 0) {
        rest = rest.substr(7);
        parts.https = false;
    }
    const std::size_t slash = rest.find(L'/');
    if (slash == std::wstring::npos) {
        parts.host = rest;
        parts.path = L"/";
    } else {
        parts.host = rest.substr(0, slash);
        parts.path = rest.substr(slash);
    }
    return parts;
}

class HInternetHolder {
public:
    explicit HInternetHolder(HINTERNET handle) : handle_(handle) {}
    ~HInternetHolder() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }
    HINTERNET get() const { return handle_; }

private:
    HINTERNET handle_;
};

}

std::int64_t WindowsDownloader::QueryRemoteSize(const std::string& url) {
    const UrlParts parts = SplitUrl(url);
    HInternetHolder session(WinHttpOpen(L"LwfDownloader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.get() == nullptr) {
        return -1;
    }
    HInternetHolder connect(WinHttpConnect(session.get(), parts.host.c_str(),
                                           parts.https ? INTERNET_DEFAULT_HTTPS_PORT
                                                       : INTERNET_DEFAULT_HTTP_PORT,
                                           0));
    if (connect.get() == nullptr) {
        return -1;
    }
    HInternetHolder request(WinHttpOpenRequest(
        connect.get(), L"HEAD", parts.path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, parts.https ? WINHTTP_FLAG_SECURE : 0));
    if (request.get() == nullptr) {
        return -1;
    }
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        return -1;
    }
    wchar_t status_code[16];
    DWORD status_size = sizeof(status_code);
    DWORD index = 0;
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status_code, &status_size, &index) ||
        std::wstring(status_code) != L"200") {
        return -1;
    }
    wchar_t content_length[32];
    DWORD length_size = sizeof(content_length);
    index = 0;
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, &content_length, &length_size, &index)) {
        return static_cast<std::int64_t>(std::wcstoull(content_length, nullptr, 10));
    }
    return -1;
}

bool WindowsDownloader::Download(const std::string& url, const std::string& destination_path,
                                 const std::string& expected_sha256, std::int64_t remote_size,
                                 const std::function<bool(const DownloadProgress&)>& progress_callback) {
    std::uint64_t existing_bytes = 0;
    std::error_code fs_ec;
    if (std::filesystem::exists(destination_path, fs_ec)) {
        existing_bytes = static_cast<std::uint64_t>(std::filesystem::file_size(destination_path, fs_ec));
        bool hash_ok = false;
        const std::string actual = lwf_sha256::HexDigestOfFile(destination_path, hash_ok);
        const Onboarding::ResumePlan plan =
            Onboarding::PlanResume(existing_bytes, static_cast<std::uint64_t>(remote_size),
                                   expected_sha256, actual);
        if (!plan.valid) {
            lwf::Log::Warning("download restart: " + plan.error);
            existing_bytes = 0;
            std::filesystem::remove(destination_path, fs_ec);
        } else if (!plan.verify_required && plan.start_offset == 0 && existing_bytes > 0) {
            return true;
        } else {
            existing_bytes = plan.start_offset;
        }
    }

    const UrlParts parts = SplitUrl(url);
    HInternetHolder session(WinHttpOpen(L"LwfDownloader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.get() == nullptr) {
        throw InjectionException(ErrorCode::DownloadFailed, "WinHttpOpen failed");
    }
    HInternetHolder connect(WinHttpConnect(session.get(), parts.host.c_str(),
                                           parts.https ? INTERNET_DEFAULT_HTTPS_PORT
                                                       : INTERNET_DEFAULT_HTTP_PORT,
                                           0));
    if (connect.get() == nullptr) {
        throw InjectionException(ErrorCode::DownloadFailed, "WinHttpConnect failed");
    }
    HInternetHolder request(WinHttpOpenRequest(
        connect.get(), L"GET", parts.path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, parts.https ? WINHTTP_FLAG_SECURE : 0));
    if (request.get() == nullptr) {
        throw InjectionException(ErrorCode::DownloadFailed, "WinHttpOpenRequest failed");
    }
    if (existing_bytes > 0) {
        const std::wstring range =
            L"Range: bytes=" + std::to_wstring(existing_bytes) + L"-";
        if (!WinHttpAddRequestHeaders(request.get(), range.c_str(),
                                      static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD)) {
            throw InjectionException(ErrorCode::DownloadFailed, "Range header rejected");
        }
    }
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        throw InjectionException(ErrorCode::DownloadFailed, "request failed");
    }

    wchar_t status_code[16];
    DWORD status_size = sizeof(status_code);
    DWORD index = 0;
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status_code, &status_size, &index);
    const std::wstring status(status_code);
    if (status != L"200" && status != L"206") {
        throw InjectionException(ErrorCode::DownloadFailed,
                                 "unexpected status " + std::string(status.begin(), status.end()));
    }

    const std::string tmp_path = destination_path + ".part";
    std::ofstream out(tmp_path, std::ios::binary | (existing_bytes > 0 ? std::ios::app : std::ios::trunc));
    if (!out.is_open()) {
        throw InjectionException(ErrorCode::DownloadFailed, "cannot open " + tmp_path);
    }

    DownloadProgress progress;
    progress.downloaded = existing_bytes;
    progress.total = remote_size > 0 ? static_cast<std::uint64_t>(remote_size) : 0;
    std::vector<char> buffer(256 * 1024);
    DWORD bytes_read = 0;
    while (WinHttpReadData(request.get(), buffer.data(),
                           static_cast<DWORD>(buffer.size()), &bytes_read) &&
           bytes_read > 0) {
        out.write(buffer.data(), bytes_read);
        if (!out.good()) {
            throw InjectionException(ErrorCode::DownloadFailed, "disk write failed");
        }
        progress.downloaded += bytes_read;
        if (progress_callback && !progress_callback(progress)) {
            throw InjectionException(ErrorCode::DownloadFailed, "download cancelled by user");
        }
    }
    out.flush();
    out.close();

    bool hash_ok = false;
    const std::string actual = lwf_sha256::HexDigestOfFile(tmp_path, hash_ok);
    if (!expected_sha256.empty()) {
        if (!hash_ok || actual != expected_sha256) {
            std::filesystem::remove(tmp_path, fs_ec);
            throw InjectionException(ErrorCode::ChecksumMismatch,
                                     "downloaded file failed verification");
        }
    } else {
        lwf::Log::Warning("downloaded without verification: " + destination_path);
    }
    std::filesystem::rename(tmp_path, destination_path, fs_ec);
    if (fs_ec) {
        throw InjectionException(ErrorCode::DownloadFailed, "cannot finalize the file");
    }
    return true;
}

}
