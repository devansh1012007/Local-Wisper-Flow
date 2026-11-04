#pragma once

#include "core/ISttProvider.h"
#include "stt/cloud/IHttpTransport.h"
#include "stt/cloud/MuseContract.h"
#include "stt/cloud/MuseRequest.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct MuseProviderSettings {
    std::string api_key;
    std::string model = MuseContract::kDefaultModel;
    std::vector<std::string> language_bias;
    std::vector<std::string> keywords;
    std::uint32_t sample_rate = 16000;
    int timeout_ms = 15000;
    std::size_t max_clips = 40;
};

class MuseProvider final : public ISttProvider {
public:
    MuseProvider(IHttpTransport& transport, MuseProviderSettings settings);

    SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                const SttCancelCheck& cancel_check) override;

private:
    IHttpTransport& transport_;
    MuseProviderSettings settings_;
};
