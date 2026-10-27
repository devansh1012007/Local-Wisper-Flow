#pragma once

#include "core/IInferenceProvider.h"

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>
#include <vector>

#include "inference.grpc.pb.h"

class GrpcInferenceProvider : public IInferenceProvider {
public:
    explicit GrpcInferenceProvider(const std::string& endpoint);

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string ProcessText(const std::string& text,
                            const std::string& system_prompt) override;

private:
    void EnsureChannel();

    std::string endpoint_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<inference::InferenceService::Stub> stub_;
};
