#pragma once

#include "stt/cloud/IHttpTransport.h"

#include <cstdint>
#include <string>
#include <vector>

struct GnaniRequestOptions {
    std::string api_key_id;
    std::string model;
    std::string language;
    std::vector<std::string> keyterms;
};

HttpRequest BuildGnaniRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                              const GnaniRequestOptions& options);
