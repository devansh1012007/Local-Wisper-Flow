#include "llm/LocalLlamaProvider.h"

#include "llama.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kThinkOpen = "<think>";
constexpr const char* kThinkClose = "</think>";

std::string TrimWhitespace(std::string text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        text.clear();
        return text;
    }
    text.erase(last + 1);
    text.erase(0, first);
    return text;
}

std::string StripThinkingBlocks(std::string text) {
    for (;;) {
        const std::size_t open = text.find(kThinkOpen);
        if (open == std::string::npos) {
            break;
        }
        const std::size_t close = text.find(kThinkClose, open);
        if (close == std::string::npos) {
            text.erase(open);
            break;
        }
        text.erase(open, close - open);
    }
    return text;
}

bool AbortRequested(void* data) {
    auto* flag = static_cast<std::atomic<bool>*>(data);
    return flag->load(std::memory_order_relaxed);
}

}  // namespace

struct LocalLlamaProvider::Impl {
    llama_model* model = nullptr;
    const llama_vocab* vocab = nullptr;
    llama_context* context = nullptr;
    std::string chat_template;
    std::vector<llama_chat_message> chat_messages;
    std::vector<char> formatted;

    explicit Impl(const std::string& model_path, std::size_t context_size, int gpu_layers) {
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = gpu_layers;
        model = llama_model_load_from_file(model_path.c_str(), model_params);
        if (model == nullptr) {
            throw std::runtime_error("LocalLlamaProvider: failed to load model '" + model_path +
                                     "'");
        }
        vocab = llama_model_get_vocab(model);

        const char* model_template = llama_model_chat_template(model, nullptr);
        chat_template = model_template != nullptr ? model_template : "chatml";

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = static_cast<std::uint32_t>(context_size);
        ctx_params.n_batch = static_cast<std::uint32_t>(context_size);
        context = llama_init_from_model(model, ctx_params);
        if (context == nullptr) {
            llama_model_free(model);
            model = nullptr;
            throw std::runtime_error("LocalLlamaProvider: failed to create inference context");
        }
    }

    ~Impl() {
        if (context != nullptr) {
            llama_free(context);
            context = nullptr;
        }
        if (model != nullptr) {
            llama_model_free(model);
            model = nullptr;
        }
    }
};

LocalLlamaProvider::LocalLlamaProvider(const std::string& model_path, Options options)
    : impl_(std::make_unique<Impl>(model_path, options.context_size, options.gpu_layers)),
      options_(options) {}

LocalLlamaProvider::~LocalLlamaProvider() = default;

void LocalLlamaProvider::RequestCancel() {
    cancel_requested_.store(true, std::memory_order_relaxed);
}

bool LocalLlamaProvider::LastGenerationTruncated() const {
    return last_truncated_.load(std::memory_order_relaxed);
}

bool LocalLlamaProvider::WarmUp() {
    try {
        Process("hi", "Reply with the single word: ready");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string LocalLlamaProvider::Process(const std::string& text, const std::string& system_prompt) {
    if (text.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    last_truncated_.store(false, std::memory_order_relaxed);
    cancel_requested_.store(false, std::memory_order_relaxed);

    llama_set_abort_callback(impl_->context, &AbortRequested, &cancel_requested_);
    llama_memory_seq_rm(llama_get_memory(impl_->context), -1, -1, -1);

    const std::string cleaned_input = TrimWhitespace(text);
    if (cleaned_input.empty()) {
        return {};
    }

    impl_->chat_messages.clear();
    llama_chat_message system_message{};
    system_message.role = "system";
    system_message.content = system_prompt.c_str();
    impl_->chat_messages.push_back(system_message);
    llama_chat_message user_message{};
    user_message.role = "user";
    user_message.content = cleaned_input.c_str();
    impl_->chat_messages.push_back(user_message);

    const std::int32_t formatted_size = llama_chat_apply_template(
        impl_->chat_template.c_str(), impl_->chat_messages.data(), impl_->chat_messages.size(),
        true, nullptr, 0);
    if (formatted_size <= 0) {
        throw std::runtime_error("LocalLlamaProvider: chat template application failed");
    }
    impl_->formatted.assign(static_cast<std::size_t>(formatted_size) + 1, '\0');
    llama_chat_apply_template(impl_->chat_template.c_str(), impl_->chat_messages.data(),
                              impl_->chat_messages.size(), true, impl_->formatted.data(),
                              static_cast<std::int32_t>(impl_->formatted.size()));

    const std::string prompt(impl_->formatted.data());

    const bool add_special = false;
    const bool parse_special = true;
    const std::int32_t n_prompt_tokens =
        -llama_tokenize(impl_->vocab, prompt.c_str(), static_cast<std::int32_t>(prompt.size()),
                        nullptr, 0, add_special, parse_special);
    if (n_prompt_tokens <= 0) {
        throw std::runtime_error("LocalLlamaProvider: tokenization failed");
    }

    std::vector<llama_token> tokens(static_cast<std::size_t>(n_prompt_tokens));
    if (llama_tokenize(impl_->vocab, prompt.c_str(), static_cast<std::int32_t>(prompt.size()),
                       tokens.data(), n_prompt_tokens, add_special,
                       parse_special) != n_prompt_tokens) {
        throw std::runtime_error("LocalLlamaProvider: tokenization count mismatch");
    }

    if (tokens.size() + options_.max_predict_tokens > options_.context_size) {
        throw std::runtime_error("LocalLlamaProvider: prompt exceeds configured context size");
    }

    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(options_.temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(options_.top_p, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(-1));

    std::string result;
    bool truncated = false;
    try {
        llama_batch prefill_batch =
            llama_batch_get_one(tokens.data(), static_cast<std::int32_t>(tokens.size()));
        const std::int32_t prefill_rc = llama_decode(impl_->context, prefill_batch);
        if (prefill_rc == 2) {
            throw std::runtime_error("LocalLlamaProvider: cancelled");
        }
        if (prefill_rc != 0) {
            throw std::runtime_error("LocalLlamaProvider: llama_decode failed during prefill");
        }

        for (std::size_t generated = 0; generated < options_.max_predict_tokens; ++generated) {
            if (cancel_requested_.load(std::memory_order_relaxed)) {
                throw std::runtime_error("LocalLlamaProvider: cancelled");
            }
            const llama_token next = llama_sampler_sample(sampler, impl_->context, -1);
            if (llama_vocab_is_eog(impl_->vocab, next)) {
                break;
            }
            if (generated + 1 == options_.max_predict_tokens) {
                truncated = true;
            }

            char piece_buffer[256];
            const int piece_len = llama_token_to_piece(impl_->vocab, next, piece_buffer,
                                                       sizeof(piece_buffer), 0, parse_special);
            if (piece_len < 0) {
                throw std::runtime_error("LocalLlamaProvider: token to piece failed");
            }
            result.append(piece_buffer, static_cast<std::size_t>(piece_len));

            llama_token next_mutable = next;
            llama_batch decode_batch = llama_batch_get_one(&next_mutable, 1);
            const std::int32_t decode_rc = llama_decode(impl_->context, decode_batch);
            if (decode_rc == 2) {
                throw std::runtime_error("LocalLlamaProvider: cancelled");
            }
            if (decode_rc != 0) {
                throw std::runtime_error(
                    "LocalLlamaProvider: llama_decode failed during generation");
            }
        }
    } catch (...) {
        llama_sampler_free(sampler);
        llama_memory_seq_rm(llama_get_memory(impl_->context), -1, -1, -1);
        throw;
    }

    llama_sampler_free(sampler);
    llama_memory_seq_rm(llama_get_memory(impl_->context), -1, -1, -1);
    last_truncated_.store(truncated, std::memory_order_relaxed);

    result = StripThinkingBlocks(std::move(result));
    return TrimWhitespace(std::move(result));
}
