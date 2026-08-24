#include "stt/LocalWhisperProvider.h"

#include "whisper.h"

#include <stdexcept>

namespace {

whisper_context* LoadModel(const std::string& model_path) {
    whisper_context_params params = whisper_context_default_params();
    whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
    if (ctx == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: failed to load model '" + model_path + "'");
    }
    return ctx;
}

}

LocalWhisperProvider::LocalWhisperProvider(const std::string& model_path)
    : ctx_(LoadModel(model_path)) {}

LocalWhisperProvider::~LocalWhisperProvider() {
    if (ctx_ != nullptr) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

std::string LocalWhisperProvider::Transcribe(const std::vector<float>& audio_pcm) {
    if (ctx_ == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: context not initialized");
    }
    if (audio_pcm.empty()) {
        return {};
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.translate = false;
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.no_context = true;

    if (whisper_full(ctx_, params, audio_pcm.data(), static_cast<int>(audio_pcm.size())) != 0) {
        throw std::runtime_error("LocalWhisperProvider: whisper_full failed");
    }

    std::string text;
    const int segment_count = whisper_full_n_segments(ctx_);
    for (int i = 0; i < segment_count; ++i) {
        const char* segment = whisper_full_get_segment_text(ctx_, i);
        if (segment != nullptr) {
            text += segment;
        }
    }
    return text;
}

std::string LocalWhisperProvider::ProcessText(const std::string& text, const std::string&) {
    return text;
}
