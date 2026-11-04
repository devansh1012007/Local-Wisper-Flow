#pragma once

#include "core/ISttProvider.h"

#include <cstdint>
#include <string>
#include <vector>

struct MuseSuccess {
    std::string transcript;
    std::string session_id;
};

struct MuseFailure {
    SttErrorCategory category = SttErrorCategory::Unknown;
    int http_status = 0;
    std::string session_id;
    std::string detail;
};

bool ParseMuseResponse(int http_status, const std::vector<std::uint8_t>& body,
                       MuseSuccess& success, MuseFailure& failure);
