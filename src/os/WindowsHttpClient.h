#pragma once

#include "stt/cloud/IHttpTransport.h"

class WindowsHttpClient final : public IHttpTransport {
public:
    HttpResponse Post(const HttpRequest& request) override;
};
