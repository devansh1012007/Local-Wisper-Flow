#include "stt/cloud/ElevenLabsRequest.h"

#include "stt/cloud/ElevenLabsContract.h"
#include "stt/cloud/MultipartBuilder.h"
#include "stt/cloud/WavEncoder.h"

#include <stdexcept>

namespace {

bool SafeKeyterm(const std::string& term) {
    if (term.empty() || term.size() > ElevenLabsContract::kKeytermMaxChars) {
        return false;
    }
    const std::string banned = "<>{}[]\\";
    for (const char c : term) {
        if (c == ',' || c == '\r' || c == '\n') {
            return false;
        }
        if (banned.find(c) != std::string::npos) {
            return false;
        }
    }
    std::size_t words = 1;
    bool in_space = false;
    for (const char c : term) {
        if (c == ' ' || c == '\t') {
            if (!in_space) {
                ++words;
            }
            in_space = true;
        } else {
            in_space = false;
        }
    }
    return words <= ElevenLabsContract::kKeytermMaxWords;
}

}  // namespace

HttpRequest BuildElevenLabsRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                                   const ElevenLabsRequestOptions& options) {
    if (options.api_key.empty()) {
        throw std::runtime_error("ElevenLabsRequest: api key is required");
    }
    if (options.model.empty()) {
        throw std::runtime_error("ElevenLabsRequest: model is required");
    }
    MultipartBuilder builder(ElevenLabsContract::kBoundary);
    builder.AddField(ElevenLabsContract::kModelField, options.model);
    if (!options.language_code.empty()) {
        builder.AddField(ElevenLabsContract::kLanguageField, options.language_code);
    }
    std::size_t used = 0;
    for (const std::string& term : options.keyterms) {
        if (used >= ElevenLabsContract::kKeytermsCap) {
            break;
        }
        if (!SafeKeyterm(term)) {
            continue;
        }
        builder.AddField(ElevenLabsContract::kKeytermsField, term);
        ++used;
    }
    builder.AddFile(ElevenLabsContract::kAudioField, ElevenLabsContract::kAudioFilename,
                    ElevenLabsContract::kAudioContentType,
                    EncodeWav16Mono(audio_pcm, sample_rate));
    HttpRequest request;
    request.host = ElevenLabsContract::kHost;
    request.path = ElevenLabsContract::kPath;
    request.headers.emplace_back(ElevenLabsContract::kApiKeyHeader, options.api_key);
    request.content_type = builder.ContentType();
    request.body = builder.Build();
    return request;
}
