#pragma once

#include "core/ISttProvider.h"

#include <cstdint>
#include <string>
#include <vector>

struct GnaniSuccess {
    std::string transcript;
    std::string request_id;
};

struct GnaniFailure {
    SttErrorCategory category = SttErrorCategory::Unknown;
    int http_status = 0;
    std::string request_id;
    std::string detail;
};

bool ParseGnaniResponse(int http_status, const std::vector<std::uint8_t>& body,
                        const std::string& request_id_header, GnaniSuccess& success,
                        GnaniFailure& failure);
