#pragma once

#include "core/ISttProvider.h"
#include "stt/cloud/ElevenLabsContract.h"
#include "stt/cloud/ElevenLabsRequest.h"
#include "stt/cloud/IHttpTransport.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ElevenLabsProviderSettings {
    std::string api_key;
    std::string model = ElevenLabsContract::kDefaultModel;
    std::string language_code;
    std::vector<std::string> keyterms;
    std::uint32_t sample_rate = 16000;
    int timeout_ms = 15000;
    std::size_t max_clips = 40;
};

class ElevenLabsProvider final : public ISttProvider {
public:
    ElevenLabsProvider(IHttpTransport& transport, ElevenLabsProviderSettings settings);

    SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                const SttCancelCheck& cancel_check) override;

private:
    IHttpTransport& transport_;
    ElevenLabsProviderSettings settings_;
};
