#include "stt/LocalWhisperProvider.h"

#include "core/Log.h"
#include "whisper.h"

#include <stdexcept>

namespace {

constexpr std::size_t kMaxPromptChars = 600;

whisper_context* LoadModel(const std::string& model_path) {
    whisper_context_params params = whisper_context_default_params();
    whisper_context* ctx = whisper_init_from_file_with_params(model_path.c_str(), params);
    if (ctx == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: failed to load model '" + model_path + "'");
    }
    return ctx;
}

std::string MergePrompt(const std::string& prefix, const std::string& context) {
    std::string combined = prefix;
    std::string ctx = context;
    while (combined.size() + ctx.size() + 1 > kMaxPromptChars && !ctx.empty()) {
        const std::size_t space = ctx.find(' ');
        if (space == std::string::npos) {
            ctx.clear();
        } else {
            ctx = ctx.substr(space + 1);
        }
    }
    if (!ctx.empty()) {
        if (!combined.empty()) {
            combined += " ";
        }
        combined += ctx;
    }
    if (combined.size() > kMaxPromptChars) {
        lwf::Log::Debug("whisper prompt truncated to " + std::to_string(kMaxPromptChars) + " chars");
        combined = combined.substr(0, kMaxPromptChars);
    }
    return combined;
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

void LocalWhisperProvider::SetPromptPrefix(std::string prefix) {
    prompt_prefix_ = std::move(prefix);
}

void LocalWhisperProvider::RequestCancel() {
    cancel_requested_.store(true);
}

std::string LocalWhisperProvider::Transcribe(const std::vector<float>& audio_pcm) {
    return RunWhisper(audio_pcm, {});
}

std::string LocalWhisperProvider::TranscribeWithPrompt(const std::vector<float>& audio_pcm,
                                                       const std::string& prompt) {
    return RunWhisper(audio_pcm, prompt);
}

std::string LocalWhisperProvider::RunWhisper(const std::vector<float>& audio_pcm,
                                             const std::string& prompt) {
    if (ctx_ == nullptr) {
        throw std::runtime_error("LocalWhisperProvider: context not initialized");
    }
    if (audio_pcm.empty()) {
        return {};
    }
    cancel_requested_.store(false);

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.translate = false;
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.no_context = true;
    params.abort_callback = [](void* data) -> bool {
        auto* flag = static_cast<std::atomic<bool>*>(data);
        return flag != nullptr && flag->load();
    };
    params.abort_callback_user_data = &cancel_requested_;

    const std::string merged = MergePrompt(prompt_prefix_, prompt);
    if (!merged.empty()) {
        params.initial_prompt = merged.c_str();
    }

    if (whisper_full(ctx_, params, audio_pcm.data(), static_cast<int>(audio_pcm.size())) != 0) {
        if (cancel_requested_.load()) {
            throw std::runtime_error("LocalWhisperProvider: transcription cancelled");
        }
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
