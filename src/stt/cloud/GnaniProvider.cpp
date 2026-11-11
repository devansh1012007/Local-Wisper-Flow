#include "stt/cloud/GnaniProvider.h"

#include "core/Log.h"
#include "core/Redact.h"
#include "stt/cloud/ClipSplitter.h"
#include "stt/cloud/GnaniResponse.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t kMaxDetailLogChars = 200;

std::string TruncateDetail(const std::string& detail) {
    // INVARIANT: byte limit is 200 but never split a multi-byte UTF-8
    // sequence; stored attempt stats stay untruncated, this is display copy.
    if (detail.size() <= kMaxDetailLogChars) {
        return detail;
    }
    std::size_t cut = kMaxDetailLogChars;
    while (cut > 0 && (static_cast<unsigned char>(detail[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    return detail.substr(0, cut);
}

void AppendText(std::string& target, const std::string& addition) {
    if (addition.empty()) {
        return;
    }
    if (!target.empty()) {
        target += " ";
    }
    target += addition;
}

std::string RequestIdOf(const HttpResponse& response) {
    for (std::size_t i = 0; i < GnaniContract::kRequestIdHeaderCount; ++i) {
        const std::string value = response.Header(GnaniContract::kRequestIdHeaders[i]);
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

void LogAttempt(const SttAttemptStats& attempt) {
    // INVARIANT: attempt.detail is already redacted by the caller and never
    // contains transcript text; truncate a display copy only, never the
    // stored attempt stats.
    const std::string detail = TruncateDetail(attempt.detail);
    std::cerr << "[stt] provider=" << attempt.provider << " request_id="
              << (attempt.request_id.empty() ? std::string("-") : attempt.request_id)
              << " status=" << attempt.http_status << " error=" << ToString(attempt.error)
              << " detail=" << detail
              << " bytes=" << attempt.bytes_sent << "/" << attempt.bytes_received
              << " latency_ms=" << attempt.latency.count() << '\n';
    lwf::Log::Warning("[stt] provider=" + attempt.provider + " request_id=" +
                      (attempt.request_id.empty() ? std::string("-") : attempt.request_id) +
                      " status=" + std::to_string(attempt.http_status) +
                      " error=" + std::string(ToString(attempt.error)) + " detail=" + detail);
}

}  // namespace

GnaniProvider::GnaniProvider(IHttpTransport& transport, GnaniProviderSettings settings)
    : transport_(transport), settings_(std::move(settings)) {}

SttTranscription GnaniProvider::Transcribe(const std::vector<float>& audio_pcm,
                                           const SttCancelCheck& cancel_check) {
    SttTranscription result;
    if (audio_pcm.empty()) {
        result.success = true;
        return result;
    }
    const std::vector<AudioClip> clips =
        SplitIntoClips(audio_pcm, settings_.sample_rate, GnaniContract::kMaxClipMs);
    GnaniRequestOptions options;
    options.api_key_id = settings_.api_key_id;
    options.model = settings_.model;
    options.language = settings_.language;
    options.keyterms = settings_.keyterms;
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
        HttpRequest request = BuildGnaniRequest(clip_pcm, settings_.sample_rate, options);
        request.timeout_ms = settings_.timeout_ms;
        SttAttemptStats attempt;
        attempt.provider = "gnani";
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
                                               settings_.api_key_id);
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
                                               settings_.api_key_id);
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
        attempt.request_id = RequestIdOf(response);
        GnaniSuccess success;
        GnaniFailure failure;
        if (ParseGnaniResponse(response.status, response.body, attempt.request_id, success,
                               failure)) {
            attempt.error = SttErrorCategory::None;
            result.attempts.push_back(attempt);
            LogAttempt(attempt);
            AppendText(text, success.transcript);
            ++requests;
            continue;
        }
        attempt.error = failure.category;
        attempt.detail = lwf::RedactSecret(lwf::RedactAuthValues(failure.detail),
                                           settings_.api_key_id);
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
