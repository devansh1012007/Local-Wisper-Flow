#include "stt/cloud/GnaniRequest.h"

#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/MultipartBuilder.h"
#include "stt/cloud/WavEncoder.h"

#include <stdexcept>

namespace {

bool SafeKeyterm(const std::string& term) {
    if (term.empty()) {
        return false;
    }
    if (term.find(',') != std::string::npos || term.find('\r') != std::string::npos ||
        term.find('\n') != std::string::npos) {
        return false;
    }
    return true;
}

}  // namespace

HttpRequest BuildGnaniRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                              const GnaniRequestOptions& options) {
    if (options.api_key_id.empty()) {
        throw std::runtime_error("GnaniRequest: api key id is required");
    }
    if (options.model.empty() || options.language.empty()) {
        throw std::runtime_error("GnaniRequest: model and language are required");
    }
    MultipartBuilder builder(GnaniContract::kBoundary);
    builder.AddField(GnaniContract::kModelField, options.model);
    builder.AddField(GnaniContract::kLanguageField, options.language);
    std::string joined_keyterms;
    std::size_t used = 0;
    for (const std::string& term : options.keyterms) {
        if (used >= GnaniContract::kKeytermsCap) {
            break;
        }
        if (!SafeKeyterm(term)) {
            continue;
        }
        if (used > 0) {
            joined_keyterms += ",";
        }
        joined_keyterms += term;
        ++used;
    }
    if (used > 0) {
        builder.AddField(GnaniContract::kKeytermsField, joined_keyterms);
    }
    builder.AddFile(GnaniContract::kAudioField, GnaniContract::kAudioFilename,
                    GnaniContract::kAudioContentType, EncodeWav16Mono(audio_pcm, sample_rate));
    HttpRequest request;
    request.host = GnaniContract::kHost;
    request.path = GnaniContract::kPath;
    request.headers.emplace_back(GnaniContract::kApiKeyHeader, options.api_key_id);
    request.content_type = builder.ContentType();
    request.body = builder.Build();
    return request;
}
