#include "stt/GrpcInferenceProvider.h"

#include <chrono>
#include <stdexcept>

namespace {

constexpr auto kTranscribeTimeout = std::chrono::seconds(30);
constexpr auto kProcessTextTimeout = std::chrono::seconds(30);
constexpr int kMaxMessageBytes = 256 * 1024 * 1024;
constexpr const char* kLanguage = "en";

void ThrowOnFailure(const grpc::Status& status) {
    if (!status.ok()) {
        throw std::runtime_error("gRPC failed: " + status.error_message());
    }
}

}  // namespace

GrpcInferenceProvider::GrpcInferenceProvider(const std::string& endpoint)
    : endpoint_(endpoint) {}

void GrpcInferenceProvider::EnsureChannel() {
    if (stub_ != nullptr) {
        return;
    }
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(kMaxMessageBytes);
    args.SetMaxSendMessageSize(kMaxMessageBytes);
    channel_ = grpc::CreateCustomChannel(endpoint_, grpc::InsecureChannelCredentials(), args);
    stub_ = inference::InferenceService::NewStub(channel_);
}

std::string GrpcInferenceProvider::Transcribe(const std::vector<float>& audio_pcm) {
    EnsureChannel();

    inference::TranscribeRequest request;
    request.set_audio_data(audio_pcm.data(), audio_pcm.size() * sizeof(float));
    request.set_language(kLanguage);

    inference::TranscribeResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kTranscribeTimeout);
    context.set_max_receive_message_size(kMaxMessageBytes);

    grpc::Status status = stub_->Transcribe(&context, request, &response);
    ThrowOnFailure(status);
    return response.text();
}

std::string GrpcInferenceProvider::ProcessText(const std::string& text,
                                               const std::string& system_prompt) {
    EnsureChannel();

    inference::ProcessRequest request;
    request.set_text(text);
    request.set_system_prompt(system_prompt);

    inference::ProcessResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + kProcessTextTimeout);

    grpc::Status status = stub_->ProcessText(&context, request, &response);
    ThrowOnFailure(status);
    return response.result();
}
