#include "core/IInferenceProvider.h"
#include "core/ISttProvider.h"
#include "core/SttRouter.h"
#include "stt/cloud/ClipSplitter.h"
#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/GnaniProvider.h"
#include "stt/cloud/GnaniRequest.h"
#include "stt/cloud/WavEncoder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
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

class LoopbackTransport final : public IHttpTransport {
public:
    explicit LoopbackTransport(int rtt_ms) : rtt_ms_(rtt_ms) {}

    HttpResponse Post(const HttpRequest& request) override {
        if (rtt_ms_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(rtt_ms_));
        }
        ++requests;
        bytes_sent_total += request.body.size();
        static const char* body = "{\"transcript\":\"clip\"}";
        HttpResponse response;
        response.status = 200;
        response.body.assign(body, body + std::strlen(body));
        response.headers.emplace_back("X-Request-Id", "bench-req");
        return response;
    }

    int requests = 0;
    std::size_t bytes_sent_total = 0;

private:
    int rtt_ms_;
};

class BenchLocal final : public IInferenceProvider {
public:
    std::string Transcribe(const std::vector<float>&) override { return "local"; }
    std::string ProcessText(const std::string& text, const std::string&) override { return text; }
};

class InstantCloud final : public ISttProvider {
public:
    SttTranscription Transcribe(const std::vector<float>&, const SttCancelCheck&) override {
        SttTranscription result;
        result.text = "cloud";
        result.success = true;
        return result;
    }
};

}

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 10;
    const int utterance_seconds = argc > 2 ? std::atoi(argv[2]) : 30;
    const int rtt_ms = argc > 3 ? std::atoi(argv[3]) : 100;
    const double price_per_hour = argc > 4 ? std::atof(argv[4]) : 0.18;
    const int sample_rate = 16000;

    GnaniProviderSettings settings;
    settings.api_key_id = "bench-key";
    GnaniRequestOptions options;
    options.api_key_id = settings.api_key_id;
    options.model = GnaniContract::kDefaultModel;
    options.language = GnaniContract::kDefaultLanguage;

    std::vector<double> encode_ms;
    std::vector<double> build_ms;
    std::vector<double> split_ms;
    std::vector<double> provider_ms;
    std::vector<double> router_ms;
    std::size_t clips = 0;
    std::size_t bytes_per_clip = 0;

    for (int run = 0; run < runs; ++run) {
        const std::vector<float> utterance = SpeechLike(sample_rate, utterance_seconds);

        auto t0 = Clock::now();
        const std::vector<std::uint8_t> wav = EncodeWav16Mono(utterance, sample_rate);
        auto t1 = Clock::now();
        HttpRequest request = BuildGnaniRequest(utterance, sample_rate, options);
        auto t2 = Clock::now();
        const std::vector<AudioClip> split =
            SplitIntoClips(utterance, sample_rate, GnaniContract::kMaxClipMs);
        auto t3 = Clock::now();
        (void)wav;
        (void)request;

        LoopbackTransport transport(rtt_ms);
        GnaniProvider provider(transport, settings);
        const SttTranscription result = provider.Transcribe(utterance, {});
        auto t4 = Clock::now();

        BenchLocal local;
        SttRoutingConfig routing;
        routing.provider = "gnani";
        routing.consent_accepted = true;
        routing.api_key_source = [] { return std::string("bench-key"); };
        SttRouter router(std::make_unique<BenchLocal>(),
                         std::make_unique<InstantCloud>(), routing);
        const std::string routed = router.Transcribe(utterance);
        auto t5 = Clock::now();
        (void)routed;

        encode_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        build_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        split_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
        provider_ms.push_back(std::chrono::duration<double, std::milli>(t4 - t3).count());
        router_ms.push_back(std::chrono::duration<double, std::milli>(t5 - t4).count());
        clips = split.size();
        bytes_per_clip = request.body.size() / (clips == 0 ? 1 : clips);
    }

    std::printf("lwf-cloud-bench runs=%d utterance=%ds rtt_ms=%d price_per_hour=%.3f\n", runs,
                utterance_seconds, rtt_ms, price_per_hour);
    std::printf("stage               p50(ms)  p95(ms)\n");
    std::printf("wav_encode          %7.2f  %7.2f\n", Percentile(encode_ms, 0.50),
                Percentile(encode_ms, 0.95));
    std::printf("request_build       %7.2f  %7.2f\n", Percentile(build_ms, 0.50),
                Percentile(build_ms, 0.95));
    std::printf("clip_split          %7.2f  %7.2f\n", Percentile(split_ms, 0.50),
                Percentile(split_ms, 0.95));
    std::printf("provider_roundtrip  %7.2f  %7.2f\n", Percentile(provider_ms, 0.50),
                Percentile(provider_ms, 0.95));
    std::printf("router_gate_overhead%7.2f  %7.2f\n", Percentile(router_ms, 0.50),
                Percentile(router_ms, 0.95));
    const double audio_hours = static_cast<double>(utterance_seconds) / 3600.0;
    std::printf("clips=%zu bytes_per_clip=%.1fKB audio_hours_per_utterance=%.5f\n", clips,
                static_cast<double>(bytes_per_clip) / 1024.0, audio_hours);
    std::printf("cost_per_utterance=$%.6f cost_per_1000_audio_hours=$%.2f\n",
                audio_hours * price_per_hour, 1000.0 * price_per_hour);
    return 0;
}
