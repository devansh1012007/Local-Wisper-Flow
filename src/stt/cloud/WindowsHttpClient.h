#pragma once

#include "stt/cloud/IHttpTransport.h"

#include <string>

// WinHTTP-backed IHttpTransport for the Windows build. Constructed in
// main.cpp and injected into the cloud providers; on Linux the providers
// only ever see test fakes, so this file compiles nowhere else.
class WindowsHttpClient final : public IHttpTransport {
public:
    WindowsHttpClient() = default;
    ~WindowsHttpClient() override;

    WindowsHttpClient(const WindowsHttpClient&) = delete;
    WindowsHttpClient& operator=(const WindowsHttpClient&) = delete;

    HttpResponse Post(const HttpRequest& request) override;
};
