#pragma once

#include "stt/cloud/IHttpTransport.h"

#include <cstdint>
#include <string>
#include <vector>

struct ElevenLabsRequestOptions {
    std::string api_key;
    std::string model;
    std::string language_code;
    std::vector<std::string> keyterms;
};

HttpRequest BuildElevenLabsRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                                   const ElevenLabsRequestOptions& options);
