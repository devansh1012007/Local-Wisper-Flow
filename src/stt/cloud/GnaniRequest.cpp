#include "stt/cloud/GnaniRequest.h"

#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/MultipartBuilder.h"
#include "stt/cloud/WavEncoder.h"

#include <stdexcept>

namespace {

bool SafeKeyterm(const std::string& term) {
    if (term.empty() || term.size() > GnaniContract::kKeytermMaxChars) {
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
    return words <= GnaniContract::kKeytermMaxWords;
}

}  // namespace

HttpRequest BuildGnaniRequest(const std::vector<float>& audio_pcm, std::uint32_t sample_rate,
                              const GnaniRequestOptions& options) {
    if (options.api_key_id.empty()) {
        throw std::runtime_error("GnaniRequest: api key id is required");
    }
    if (options.model.empty()) {
        throw std::runtime_error("GnaniRequest: model is required");
    }
    MultipartBuilder builder(GnaniContract::kBoundary);
    builder.AddField(GnaniContract::kModelField, options.model);
    if (!options.language.empty()) {
        builder.AddField(GnaniContract::kLanguageField, options.language);
    }
    std::size_t used = 0;
    for (const std::string& term : options.keyterms) {
        if (used >= GnaniContract::kKeytermsCap) {
            break;
        }
        if (!SafeKeyterm(term)) {
            continue;
        }
        builder.AddField(GnaniContract::kKeytermsField, term);
        ++used;
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
