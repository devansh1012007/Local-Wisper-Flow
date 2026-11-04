#include "stt/cloud/MuseRequest.h"

#include "stt/cloud/MuseContract.h"
#include "stt/cloud/MultipartBuilder.h"
#include "stt/cloud/WavEncoder.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {

bool SafeKeyword(const std::string& term) {
    if (term.empty() || term.size() > MuseContract::kKeywordMaxChars) {
        return false;
    }
    for (const char c : term) {
        if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
    }
    return true;
}

}  // namespace

HttpRequest BuildMuseRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                             const MuseRequestOptions& options) {
    if (options.api_key.empty()) {
        throw std::runtime_error("MuseRequest: api key is required");
    }
    if (options.model.empty()) {
        throw std::runtime_error("MuseRequest: model is required");
    }
    nlohmann::json request_json;
    request_json["mode"] = MuseContract::kMode;
    request_json["model"] = options.model;
    request_json["audioEncoding"] = MuseContract::kAudioEncoding;
    if (!options.language_bias.empty()) {
        request_json["languageBias"] = options.language_bias;
    }
    std::size_t used = 0;
    for (const std::string& term : options.keywords) {
        if (used >= MuseContract::kKeywordsCap) {
            break;
        }
        if (!SafeKeyword(term)) {
            continue;
        }
        request_json["keywords"].push_back(term);
        ++used;
    }
    MultipartBuilder builder(MuseContract::kBoundary);
    builder.AddJsonField(MuseContract::kRequestField, request_json.dump());
    builder.AddFile(MuseContract::kAudioField, MuseContract::kAudioFilename,
                    MuseContract::kAudioContentType, EncodeWav16Mono(audio_pcm, sample_rate));
    HttpRequest request;
    request.host = MuseContract::kHost;
    request.path = MuseContract::kPath;
    request.headers.emplace_back(MuseContract::kApiKeyHeader, "Bearer " + options.api_key);
    request.content_type = builder.ContentType();
    request.body = builder.Build();
    return request;
}
