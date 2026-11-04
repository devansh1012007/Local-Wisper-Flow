#pragma once

#include "core/ISttProvider.h"

#include <cstdint>
#include <string>
#include <vector>

struct ElevenLabsSuccess {
    std::string text;
    std::string language_code;
    std::string request_id;
};

struct ElevenLabsFailure {
    SttErrorCategory category = SttErrorCategory::Unknown;
    int http_status = 0;
    std::string request_id;
    std::string detail;
};

bool ParseElevenLabsResponse(int http_status, const std::vector<std::uint8_t>& body,
                             const std::string& request_id_header, ElevenLabsSuccess& success,
                             ElevenLabsFailure& failure);
