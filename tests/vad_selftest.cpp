#include "core/ConfigManager.h"
#include "vad/SileroVadProvider.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kChunksPerSignal = 10;
constexpr double kSineFrequencyHz = 440.0;
constexpr double kSampleRate = 16000.0;

const char* kSpeechSampleHint =
    "curl -L -o models/jfk.wav "
    "https://raw.githubusercontent.com/ggml-org/whisper.cpp/v1.9.3/samples/jfk.wav";

std::vector<float> MakeNoiseChunk(unsigned int& seed) {
    std::vector<float> chunk(SileroVadProvider::kRequiredChunkSamples);
    for (float& sample : chunk) {
        seed = seed * 1103515245u + 12345u;
        const float unit = static_cast<float>((seed >> 16) & 0x7FFFu) / 16383.5f - 1.0f;
        sample = 0.25f * unit;
    }
    return chunk;
}

std::vector<float> MakeSineChunk() {
    std::vector<float> chunk(SileroVadProvider::kRequiredChunkSamples);
    for (std::size_t i = 0; i < chunk.size(); ++i) {
        chunk[i] = static_cast<float>(
            0.6 * std::sin(2.0 * M_PI * kSineFrequencyHz * static_cast<double>(i) / kSampleRate));
    }
    return chunk;
}

std::vector<float> LoadPcm16Mono16k(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("missing speech sample '") + path +
                                 "'. Download it with:\n  " + kSpeechSampleHint);
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<std::size_t>(size));
    if (!file.read(bytes.data(), size)) {
        throw std::runtime_error("failed to read '" + path + "'");
    }

    if (size < 44 || std::string(bytes.data(), 4) != "RIFF") {
        throw std::runtime_error("'" + path + "' is not a RIFF/WAVE file");
    }

    std::size_t data_pos = 0;
    for (std::size_t i = 12; i + 8 <= bytes.size(); ++i) {
        if (std::string(bytes.data() + i, 4) == "data") {
            data_pos = i + 8;
            break;
        }
    }
    if (data_pos == 0 || data_pos >= bytes.size()) {
        throw std::runtime_error("'" + path + "' has no data chunk");
    }

    std::vector<float> samples((bytes.size() - data_pos) / 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        auto sample16 = static_cast<std::int16_t>(
            static_cast<unsigned char>(bytes[data_pos + i * 2]) |
            (static_cast<int>(static_cast<unsigned char>(bytes[data_pos + i * 2 + 1])) << 8));
        samples[i] = static_cast<float>(sample16) / 32768.0f;
    }
    return samples;
}

int CountSpeech(SileroVadProvider& vad, bool sine) {
    unsigned int seed = 12345u;
    int hits = 0;
    for (int i = 0; i < kChunksPerSignal; ++i) {
        const std::vector<float> chunk = sine ? MakeSineChunk() : MakeNoiseChunk(seed);
        if (vad.IsSpeech(chunk)) {
            ++hits;
        }
    }
    return hits;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string model_path = argc > 1 ? argv[1] : "models/silero_vad.onnx";
    const std::string speech_path = argc > 2 ? argv[2] : "models/jfk.wav";

    float threshold = 0.6f;
    try {
        ConfigManager config("config.json");
        threshold = config.VadThreshold();
        std::cout << "[selftest] threshold from config.json: " << threshold << '\n';
    } catch (const std::exception&) {
        std::cout << "[selftest] config.json unavailable, using default threshold " << threshold
                  << '\n';
    }

    try {
        SileroVadProvider vad(model_path, threshold);
        std::cout << "[selftest] model loaded: " << model_path << '\n';

        const int noise_speech = CountSpeech(vad, false);
        std::cout << "[selftest] random noise detected as speech: " << noise_speech << "/"
                  << kChunksPerSignal << '\n';

        const int sine_speech = CountSpeech(vad, true);
        std::cout << "[selftest] pure sine detected as speech (informational): " << sine_speech
                  << "/" << kChunksPerSignal << " (Silero does not treat tones as speech)\n";

        const std::vector<float> speech_samples = LoadPcm16Mono16k(speech_path);
        int total_windows = 0;
        int real_speech = 0;
        for (std::size_t begin = 0;
             begin + SileroVadProvider::kRequiredChunkSamples <= speech_samples.size();
             begin += SileroVadProvider::kRequiredChunkSamples) {
            const std::vector<float> window(speech_samples.begin() + static_cast<long>(begin),
                                            speech_samples.begin() +
                                                static_cast<long>(begin +
                                                                  SileroVadProvider::
                                                                      kRequiredChunkSamples));
            ++total_windows;
            if (vad.IsSpeech(window)) {
                ++real_speech;
            }
        }
        const double speech_ratio =
            total_windows > 0 ? static_cast<double>(real_speech) / total_windows : 0.0;
        std::cout << "[selftest] real speech (jfk.wav): " << real_speech << "/" << total_windows
                  << " chunks (" << std::fixed << std::setprecision(1) << 100.0 * speech_ratio
                  << "% of file flagged as speech)\n";

        if (noise_speech == 0 && total_windows > 0 && speech_ratio >= 0.25) {
            std::cout << "[selftest] PASS" << std::endl;
            return 0;
        }
        std::cerr << "[selftest] FAIL" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[selftest] Error: " << e.what() << std::endl;
        return 1;
    }
}
