#pragma once

#include "core/ISttProvider.h"
#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/GnaniRequest.h"
#include "stt/cloud/IHttpTransport.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct GnaniProviderSettings {
    std::string api_key_id;
    std::string model = GnaniContract::kDefaultModel;
    std::string language = GnaniContract::kDefaultLanguage;
    std::vector<std::string> keyterms;
    std::uint32_t sample_rate = 16000;
    int timeout_ms = 15000;
    std::size_t max_clips = 40;
};

class GnaniProvider final : public ISttProvider {
public:
    GnaniProvider(IHttpTransport& transport, GnaniProviderSettings settings);

    SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                const SttCancelCheck& cancel_check) override;

private:
    IHttpTransport& transport_;
    GnaniProviderSettings settings_;
};
