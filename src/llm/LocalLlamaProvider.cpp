#include "llm/LocalLlamaProvider.h"

#include "llama.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kTemperature = 0.2;
constexpr double kTopP = 0.9;
constexpr const char* kThinkOpen = "<think>";
constexpr const char* kThinkClose = "</think>";

std::string BuildPrompt(const std::string& text, const std::string& system_prompt) {
    return system_prompt + "\n\n" + text + "\n\nTransformed text:";
}

std::string StripThinkingBlocks(std::string text) {
    const std::size_t close_len = std::char_traits<char>::length(kThinkClose);
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
        text.erase(open, close + close_len - open);
    }
    return text;
}

void TrimWhitespace(std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
        text.clear();
        return;
    }
    text.erase(last + 1);
    text.erase(0, first);
}

}  // namespace

struct LocalLlamaProvider::Impl {
    llama_model* model = nullptr;
    const llama_vocab* vocab = nullptr;
    std::size_t context_size = 2048;

    explicit Impl(const std::string& model_path, std::size_t ctx_size)
        : context_size(ctx_size) {
        llama_model_params model_params = llama_model_default_params();
        model = llama_model_load_from_file(model_path.c_str(), model_params);
        if (model == nullptr) {
            throw std::runtime_error("LocalLlamaProvider: failed to load model '" + model_path +
                                     "'");
        }
        vocab = llama_model_get_vocab(model);
    }

    ~Impl() {
        if (model != nullptr) {
            llama_model_free(model);
            model = nullptr;
        }
    }
};

LocalLlamaProvider::LocalLlamaProvider(const std::string& model_path, std::size_t context_size,
                                       std::size_t max_predict_tokens)
    : impl_(std::make_unique<Impl>(model_path, context_size)),
      context_size_(context_size),
      max_predict_tokens_(max_predict_tokens) {}

LocalLlamaProvider::~LocalLlamaProvider() = default;

std::string LocalLlamaProvider::Process(const std::string& text,
                                        const std::string& system_prompt) {
    if (text.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const std::string prompt = BuildPrompt(text, system_prompt);

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = static_cast<std::uint32_t>(impl_->context_size);
    ctx_params.n_batch = static_cast<std::uint32_t>(impl_->context_size);
    llama_context* ctx = llama_init_from_model(impl_->model, ctx_params);
    if (ctx == nullptr) {
        throw std::runtime_error("LocalLlamaProvider: failed to create inference context");
    }

    std::string result;
    llama_sampler* sampler = nullptr;

    try {
        const bool add_special = true;
        const bool parse_special = false;
        const int n_prompt_tokens =
            -llama_tokenize(impl_->vocab, prompt.c_str(),
                            static_cast<std::int32_t>(prompt.size()), nullptr, 0, add_special,
                            parse_special);

        std::vector<llama_token> tokens(static_cast<std::size_t>(n_prompt_tokens));
        if (llama_tokenize(impl_->vocab, prompt.c_str(),
                           static_cast<std::int32_t>(prompt.size()), tokens.data(),
                           n_prompt_tokens, add_special,
                           parse_special) != n_prompt_tokens) {
            throw std::runtime_error("LocalLlamaProvider: tokenization count mismatch");
        }

        if (tokens.size() + max_predict_tokens_ > impl_->context_size) {
            throw std::runtime_error(
                "LocalLlamaProvider: prompt exceeds configured context size");
        }

        sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(sampler,
                                llama_sampler_init_temp(static_cast<float>(kTemperature)));
        llama_sampler_chain_add(sampler,
                                llama_sampler_init_top_p(static_cast<float>(kTopP), 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_dist(-1));

        llama_batch prefill_batch =
            llama_batch_get_one(tokens.data(), static_cast<std::int32_t>(tokens.size()));
        if (llama_decode(ctx, prefill_batch) != 0) {
            throw std::runtime_error("LocalLlamaProvider: llama_decode failed during prefill");
        }

        for (std::size_t generated = 0; generated < max_predict_tokens_; ++generated) {
            const llama_token next = llama_sampler_sample(sampler, ctx, -1);
            if (llama_vocab_is_eog(impl_->vocab, next)) {
                break;
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
            if (llama_decode(ctx, decode_batch) != 0) {
                throw std::runtime_error("LocalLlamaProvider: llama_decode failed during "
                                         "generation");
            }
        }

        llama_sampler_free(sampler);
        sampler = nullptr;
        llama_free(ctx);
    } catch (...) {
        if (sampler != nullptr) {
            llama_sampler_free(sampler);
        }
        llama_free(ctx);
        throw;
    }

    result = StripThinkingBlocks(std::move(result));
    TrimWhitespace(result);
    return result;
}
