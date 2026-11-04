#pragma once

#include "stt/cloud/IHttpTransport.h"

#include <cstdint>
#include <string>
#include <vector>

struct MuseRequestOptions {
    std::string api_key;
    std::string model;
    std::vector<std::string> language_bias;
    std::vector<std::string> keywords;
};

HttpRequest BuildMuseRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                             const MuseRequestOptions& options);
