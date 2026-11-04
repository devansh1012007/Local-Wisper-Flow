#include "core/DictationCleanup.h"
#include "core/IInferenceProvider.h"
#include "core/ILlmProvider.h"
#include "core/PersonalDictionary.h"
#include "core/SnippetStore.h"
#include "core/SpokenCommandProcessor.h"
#include "core/TextPipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double Percentile(std::vector<double>& samples, double fraction) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const std::size_t index = static_cast<std::size_t>(fraction *
                                                       static_cast<double>(samples.size() - 1));
    return samples[index];
}

std::vector<float> SpeechLike(int sample_rate, double seconds) {
    const std::size_t total = static_cast<std::size_t>(static_cast<double>(sample_rate) * seconds);
    std::vector<float> pcm(total, 0.0f);
    const std::size_t word_samples = static_cast<std::size_t>(0.35 * sample_rate);
    const std::size_t gap_samples = static_cast<std::size_t>(0.15 * sample_rate);
    std::size_t pos = 0;
    unsigned state = 7 * 2654435761u + 1u;
    while (pos < total) {
        for (std::size_t i = 0; i < word_samples && pos + i < total; ++i) {
            const double t = static_cast<double>(i) / sample_rate;
            state = state * 1103515245u + 12345u;
            const float noise = static_cast<float>(static_cast<double>(state % 1000) / 1000.0 - 0.5);
            pcm[pos + i] = 0.25f * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 *
                                                               180.0 * t)) +
                           0.05f * noise;
        }
        pos += word_samples + gap_samples;
    }
    return pcm;
}

// Head/tail silence trim: cut samples below the threshold at both ends.
std::vector<float> TrimHeadTail(const std::vector<float>& pcm, float threshold) {
    std::size_t begin = 0;
    std::size_t end = pcm.size();
    while (begin < end && std::fabs(pcm[begin]) <= threshold) {
        ++begin;
    }
    while (end > begin && std::fabs(pcm[end - 1]) <= threshold) {
        --end;
    }
    return std::vector<float>(pcm.begin() + static_cast<std::ptrdiff_t>(begin),
                              pcm.begin() + static_cast<std::ptrdiff_t>(end));
}

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override { return text; }
};

// CPU-bound synthetic STT proxy; workload_units scales the busy loop so the
// stage can be inflated to measure the surrounding glue at realistic costs.
class SyntheticStt final : public IInferenceProvider {
public:
    explicit SyntheticStt(int workload_units) : workload_(workload_units) {}

    std::string Transcribe(const std::vector<float>& pcm) override {
        double sink = 0.0;
        for (int unit = 0; unit < workload_; ++unit) {
            for (std::size_t i = 0; i < pcm.size(); i += 16) {
                sink += static_cast<double>(pcm[i]) * 0.5;
            }
        }
        return sink == -1.0 ? std::string() : std::string("synthetic transcript");
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        return text;
    }

private:
    int workload_;
};

}  // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 20;
    const int utterance_seconds = argc > 2 ? std::atoi(argv[2]) : 5;
    const int workload_units = argc > 3 ? std::atoi(argv[3]) : 6;
    const int sample_rate = 16000;

    PassthroughLlm llm;
    DictationCleanup cleanup(&llm);
    cleanup.SetEnabled(true);
    cleanup.SetMinWords(4);
    cleanup.SetTimeoutMs(2000);
    TextPipeline pipeline(cleanup);

    SpokenCommandProcessor spoken;
    pipeline.SetSpokenCommands(&spoken);
    PersonalDictionary dictionary;
    pipeline.SetDictionary(&dictionary);
    SnippetStore snippets;
    pipeline.SetSnippets(&snippets);

    SyntheticStt stt(workload_units);

    std::vector<double> trim_ms;
    std::vector<double> stt_ms;
    std::vector<double> cleanup_ms;
    std::vector<double> total_ms;

    for (int run = 0; run < runs; ++run) {
        const std::vector<float> utterance = SpeechLike(sample_rate, utterance_seconds);

        auto t0 = Clock::now();
        const std::vector<float> trimmed = TrimHeadTail(utterance, 0.01f);
        auto t1 = Clock::now();
        const std::string raw = stt.Transcribe(trimmed);
        auto t2 = Clock::now();
        const std::string cleaned = pipeline.Process(raw);
        auto t3 = Clock::now();
        (void)cleaned;

        trim_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        stt_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        cleanup_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
        total_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t0).count());
    }

    std::printf("lwf-bench runs=%d utterance=%ds workload_units=%d\n", runs, utterance_seconds,
                workload_units);
    std::printf("stage   p50(ms)  p95(ms)\n");
    std::printf("trim    %7.2f  %7.2f\n", Percentile(trim_ms, 0.50), Percentile(trim_ms, 0.95));
    std::printf("stt     %7.2f  %7.2f\n", Percentile(stt_ms, 0.50), Percentile(stt_ms, 0.95));
    std::printf("cleanup %7.2f  %7.2f\n", Percentile(cleanup_ms, 0.50),
                Percentile(cleanup_ms, 0.95));
    std::printf("total   %7.2f  %7.2f\n", Percentile(total_ms, 0.50), Percentile(total_ms, 0.95));
    return 0;
}
