#include "core/AudioGain.h"
#include "core/DictationCleanup.h"
#include "core/IInferenceProvider.h"
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

std::vector<float> MakeUtterance(int sample_rate, double seconds, unsigned seed) {
    const std::size_t total = static_cast<std::size_t>(static_cast<double>(sample_rate) * seconds);
    std::vector<float> pcm(total, 0.0f);
    const double word_seconds = 0.35;
    const double gap_seconds = 0.15;
    const std::size_t word_samples = static_cast<std::size_t>(word_seconds * sample_rate);
    const std::size_t gap_samples = static_cast<std::size_t>(gap_seconds * sample_rate);
    std::size_t pos = 0;
    unsigned state = seed * 2654435761u + 1u;
    while (pos < total) {
        for (std::size_t i = 0; i < word_samples && pos + i < total; ++i) {
            const double t = static_cast<double>(i) / sample_rate;
            state = state * 1103515245u + 12345u;
            const float noise = static_cast<float>(static_cast<double>(state % 1000) / 1000.0 - 0.5);
            pcm[pos + i] = 0.25f * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * 180.0 *
                                                               t)) +
                           0.05f * noise;
        }
        pos += word_samples + gap_samples;
    }
    return pcm;
}

std::size_t TrimHeadTailSilence(const std::vector<float>& pcm) {
    const float threshold = 0.01f;
    std::size_t begin = 0;
    std::size_t end = pcm.size();
    while (begin < end && std::fabs(pcm[begin]) < threshold) {
        ++begin;
    }
    while (end > begin && std::fabs(pcm[end - 1]) < threshold) {
        --end;
    }
    return end - begin;
}

class SyntheticStt final : public IInferenceProvider {
public:
    explicit SyntheticStt(int workload_units) : workload_units_(workload_units) {}

    std::string Transcribe(const std::vector<float>& pcm) override {
        volatile double acc = 0.0;
        for (std::size_t i = 0; i < pcm.size(); i += 4) {
            for (int u = 0; u < workload_units_; ++u) {
                acc += std::sqrt(static_cast<double>(i % 97) + 1.0);
            }
        }
        (void)acc;
        return "synthetic transcript of the utterance for benchmark purposes";
    }

    std::string ProcessText(const std::string& text, const std::string&) override { return text; }

private:
    int workload_units_;
};

class BenchLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override {
        volatile double acc = 0.0;
        for (int i = 0; i < 20000; ++i) {
            acc += std::sqrt(static_cast<double>(i % 53) + 1.0);
        }
        (void)acc;
        return text;
    }
};

}

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 20;
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 5;
    const int sample_rate = 16000;
    const int workload_units = argc > 3 ? std::atoi(argv[3]) : 6;

    SyntheticStt stt(workload_units);
    BenchLlm llm;
    DictationCleanup cleanup(&llm);
    cleanup.SetEnabled(true);
    cleanup.SetMinWords(4);
    cleanup.SetTimeoutMs(2000);
    TextPipeline pipeline(cleanup);

    std::vector<double> trim_ms;
    std::vector<double> stt_ms;
    std::vector<double> cleanup_ms;
    std::vector<double> total_ms;

    for (int run = 0; run < runs; ++run) {
        const std::vector<float> utterance = MakeUtterance(sample_rate, seconds, run + 1);
        const auto t0 = Clock::now();
        const std::size_t kept = TrimHeadTailSilence(utterance);
        const auto t1 = Clock::now();
        const std::string raw = stt.Transcribe(utterance);
        const auto t2 = Clock::now();
        const std::string text = pipeline.Process(raw);
        const auto t3 = Clock::now();

        trim_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        stt_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        cleanup_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
        total_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t0).count());
        (void)kept;
    }

    std::printf("lwf-bench runs=%d utterance=%ds sample_rate=%d workload_units=%d\n", runs, seconds,
                sample_rate, workload_units);
    std::printf("stage      p50(ms)  p95(ms)\n");
    std::printf("trim       %7.2f  %7.2f\n", Percentile(trim_ms, 0.50), Percentile(trim_ms, 0.95));
    std::printf("stt        %7.2f  %7.2f\n", Percentile(stt_ms, 0.50), Percentile(stt_ms, 0.95));
    std::printf("cleanup    %7.2f  %7.2f\n", Percentile(cleanup_ms, 0.50),
                Percentile(cleanup_ms, 0.95));
    std::printf("total      %7.2f  %7.2f\n", Percentile(total_ms, 0.50),
                Percentile(total_ms, 0.95));
    return 0;
}
