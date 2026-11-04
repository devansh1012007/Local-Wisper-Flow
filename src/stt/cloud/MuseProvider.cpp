#include "stt/cloud/MuseProvider.h"

#include "core/Redact.h"
#include "stt/cloud/ClipSplitter.h"
#include "stt/cloud/MuseResponse.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

void AppendText(std::string& target, const std::string& addition) {
    if (addition.empty()) {
        return;
    }
    if (!target.empty()) {
        target += " ";
    }
    target += addition;
}

void LogAttempt(const SttAttemptStats& attempt) {
    std::cerr << "[stt] provider=" << attempt.provider << " request_id="
              << (attempt.request_id.empty() ? std::string("-") : attempt.request_id)
              << " status=" << attempt.http_status << " error=" << ToString(attempt.error)
              << " bytes=" << attempt.bytes_sent << "/" << attempt.bytes_received
              << " latency_ms=" << attempt.latency.count() << '\n';
}

}  // namespace

MuseProvider::MuseProvider(IHttpTransport& transport, MuseProviderSettings settings)
    : transport_(transport), settings_(std::move(settings)) {}

SttTranscription MuseProvider::Transcribe(const std::vector<float>& audio_pcm,
                                          const SttCancelCheck& cancel_check) {
    SttTranscription result;
    if (audio_pcm.empty()) {
        result.success = true;
        return result;
    }
    const std::vector<AudioClip> clips =
        SplitIntoClips(audio_pcm, settings_.sample_rate, MuseContract::kMaxClipMs);
    MuseRequestOptions options;
    options.api_key = settings_.api_key;
    options.model = settings_.model;
    options.language_bias = settings_.language_bias;
    options.keywords = settings_.keywords;
    std::string text;
    std::size_t requests = 0;
    for (const AudioClip& clip : clips) {
        if (requests >= settings_.max_clips) {
            break;
        }
        if (cancel_check && cancel_check()) {
            result.cancelled = true;
            result.error = SttErrorCategory::Cancelled;
            result.text.clear();
            return result;
        }
        const std::vector<float> clip_pcm(audio_pcm.begin() + static_cast<std::ptrdiff_t>(clip.begin),
                                          audio_pcm.begin() +
                                              static_cast<std::ptrdiff_t>(clip.begin + clip.frames));
        HttpRequest request = BuildMuseRequest(clip_pcm, settings_.sample_rate, options);
        request.timeout_ms = settings_.timeout_ms;
        SttAttemptStats attempt;
        attempt.provider = "muse";
        attempt.bytes_sent = request.body.size();
        const auto started = std::chrono::steady_clock::now();
        HttpResponse response;
        try {
            response = transport_.Post(request);
        } catch (const HttpTransportError& error) {
            attempt.error = error.Kind() == HttpTransportErrorKind::Timeout
                                ? SttErrorCategory::Timeout
                                : SttErrorCategory::Network;
            attempt.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            attempt.detail = lwf::RedactSecret(lwf::RedactAuthValues(error.what()),
                                               settings_.api_key);
            result.attempts.push_back(attempt);
            LogAttempt(attempt);
            result.error = attempt.error;
            result.text.clear();
            return result;
        } catch (const std::exception& error) {
            attempt.error = SttErrorCategory::Network;
            attempt.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            attempt.detail = lwf::RedactSecret(lwf::RedactAuthValues(error.what()),
                                               settings_.api_key);
            result.attempts.push_back(attempt);
            LogAttempt(attempt);
            result.error = attempt.error;
            result.text.clear();
            return result;
        }
        attempt.http_status = response.status;
        attempt.bytes_received = response.body.size();
        attempt.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        MuseSuccess success;
        MuseFailure failure;
        if (ParseMuseResponse(response.status, response.body, success, failure)) {
            attempt.error = SttErrorCategory::None;
            attempt.request_id = success.session_id;
            result.attempts.push_back(attempt);
            LogAttempt(attempt);
            AppendText(text, success.transcript);
            ++requests;
            continue;
        }
        attempt.error = failure.category;
        attempt.request_id = failure.session_id;
        attempt.detail = lwf::RedactSecret(lwf::RedactAuthValues(failure.detail),
                                           settings_.api_key);
        result.attempts.push_back(attempt);
        LogAttempt(attempt);
        result.error = failure.category;
        result.text.clear();
        return result;
    }
    result.text = text;
    result.success = true;
    result.error = SttErrorCategory::None;
    return result;
}
