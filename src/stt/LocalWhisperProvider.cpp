#include "stt/LocalWhisperProvider.h"

#include "core/WhisperText.h"
#include "whisper.h"

#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t kWarmUpSamples = 16000 / 10;

whisper_context* LoadModel(const std::string& model_path) {
    whisper_context_params params = whisper_context_default_params();
    whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
    if (ctx == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: failed to load model '" + model_path + "'");
    }
    return ctx;
}

}  // namespace

LocalWhisperProvider::LocalWhisperProvider(const std::string& model_path, WhisperSettings settings)
    : ctx_(LoadModel(model_path)), settings_(std::move(settings)) {}

LocalWhisperProvider::~LocalWhisperProvider() {
    if (ctx_ != nullptr) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

void LocalWhisperProvider::SetInitialPrompt(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(mutex_);
    initial_prompt_ = prompt;
}

void LocalWhisperProvider::WarmUp() {
    Transcribe(std::vector<float>(kWarmUpSamples, 0.0f));
}

std::string LocalWhisperProvider::Transcribe(const std::vector<float>& audio_pcm) {
    if (ctx_ == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: context not initialized");
    }
    if (audio_pcm.empty()) {
        return {};
    }

    std::string initial_prompt;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        initial_prompt = initial_prompt_;
    }

    whisper_full_params params = settings_.beam_size > 1
                                     ? whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH)
                                     : whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.translate = false;
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.no_context = true;
    params.no_timestamps = true;
    params.suppress_nst = settings_.suppress_non_speech_tokens;
    params.n_threads = settings_.threads;
    if (settings_.beam_size > 1) {
        params.beam_search.beam_size = settings_.beam_size;
    }
    if (settings_.disable_temperature_fallback) {
        params.temperature_inc = 0.0f;
    }
    if (settings_.language.empty() || settings_.language == "auto") {
        params.language = nullptr;
    } else {
        params.language = settings_.language.c_str();
    }
    if (!initial_prompt.empty()) {
        params.initial_prompt = initial_prompt.c_str();
    }

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
    return WhisperText::FilterHallucinations(text);
}

std::string LocalWhisperProvider::ProcessText(const std::string& text, const std::string&) {
    return text;
}
