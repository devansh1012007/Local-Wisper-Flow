#include "vad/SileroVadProvider.h"

#include <onnxruntime_cxx_api.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::int64_t kSampleRate = 16000;
constexpr std::size_t kStateElements = 256;

constexpr const char* kInputNames[] = {"input", "state", "sr"};
constexpr const char* kOutputNames[] = {"output", "stateN"};

std::vector<char> ReadModelFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("SileroVadProvider: cannot open model file '" + path + "'");
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("SileroVadProvider: model file '" + path + "' is empty");
    }
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("SileroVadProvider: failed to read model file '" + path + "'");
    }
    return buffer;
}

bool InputNameMatches(Ort::Session& session, Ort::Allocator& allocator, std::size_t index,
                      const char* expected) {
    Ort::AllocatedStringPtr actual = session.GetInputNameAllocated(index, allocator);
    return std::strcmp(actual.get(), expected) == 0;
}

bool OutputNameMatches(Ort::Session& session, Ort::Allocator& allocator, std::size_t index,
                       const char* expected) {
    Ort::AllocatedStringPtr actual = session.GetOutputNameAllocated(index, allocator);
    return std::strcmp(actual.get(), expected) == 0;
}

}  // namespace

struct SileroVadProvider::Impl {
    Ort::Env env;
    Ort::Session session{nullptr};
    std::vector<float> state = std::vector<float>(kStateElements, 0.0f);

    explicit Impl(const std::string& model_path)
        : env(ORT_LOGGING_LEVEL_WARNING, "LocalWisperFlow") {
        const std::vector<char> model = ReadModelFile(model_path);

        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

        session = Ort::Session(env, model.data(), model.size(), options);

        if (session.GetInputCount() != 3 || session.GetOutputCount() != 2) {
            throw std::runtime_error(
                "SileroVadProvider: unsupported silero_vad.onnx signature "
                "(expected Silero v5 layout with inputs input/state/sr)");
        }

        Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Allocator allocator(session, memory);
        if (!InputNameMatches(session, allocator, 0, kInputNames[0]) ||
            !InputNameMatches(session, allocator, 1, kInputNames[1]) ||
            !InputNameMatches(session, allocator, 2, kInputNames[2]) ||
            !OutputNameMatches(session, allocator, 0, kOutputNames[0]) ||
            !OutputNameMatches(session, allocator, 1, kOutputNames[1])) {
            throw std::runtime_error(
                "SileroVadProvider: silero_vad.onnx tensor names do not match the expected v5 layout");
        }
    }
};

SileroVadProvider::SileroVadProvider(const std::string& model_path, float speech_threshold)
    : impl_(std::make_unique<Impl>(model_path)), threshold_(speech_threshold) {}

SileroVadProvider::~SileroVadProvider() = default;

bool SileroVadProvider::IsSpeech(const std::vector<float>& audio_chunk) {
    if (audio_chunk.size() != kRequiredChunkSamples) {
        throw std::invalid_argument("SileroVadProvider: chunk must contain exactly 512 samples");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    const std::int64_t input_shape[] = {1, static_cast<std::int64_t>(audio_chunk.size())};
    Ort::Value input_tensor =
        Ort::Value::CreateTensor<float>(memory, const_cast<float*>(audio_chunk.data()),
                                        audio_chunk.size(), input_shape, 2);

    const std::int64_t state_shape[] = {2, 1, 128};
    Ort::Value state_tensor = Ort::Value::CreateTensor<float>(
        memory, impl_->state.data(), impl_->state.size(), state_shape, 3);

    std::int64_t sample_rate = kSampleRate;
    Ort::Value sr_tensor =
        Ort::Value::CreateTensor<std::int64_t>(memory, &sample_rate, 1, nullptr, 0);

    std::vector<Ort::Value> inputs;
    inputs.reserve(3);
    inputs.push_back(std::move(input_tensor));
    inputs.push_back(std::move(state_tensor));
    inputs.push_back(std::move(sr_tensor));

    std::vector<Ort::Value> outputs = impl_->session.Run(
        Ort::RunOptions{nullptr}, kInputNames, inputs.data(), inputs.size(), kOutputNames,
        sizeof(kOutputNames) / sizeof(kOutputNames[0]));

    if (outputs.size() != 2) {
        throw std::runtime_error("SileroVadProvider: unexpected inference output count");
    }

    std::memcpy(impl_->state.data(), outputs[1].GetTensorData<float>(),
                kStateElements * sizeof(float));

    return outputs[0].GetTensorData<float>()[0] > threshold_;
}
