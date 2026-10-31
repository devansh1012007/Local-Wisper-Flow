#pragma once

#include "core/IVadProvider.h"

#include <chrono>
#include <cstddef>
#include <vector>

class VadGate {
public:
    static constexpr std::size_t kChunkSamples = 512;
    static constexpr std::size_t kHeadPadSamples = 2560;
    static constexpr std::size_t kTailPadSamples = 3840;

    explicit VadGate(IVadProvider* vad)
        : vad_(vad) {}

    void Reset() {
        pending_.clear();
        total_samples_ = 0;
        first_speech_sample_ = 0;
        last_speech_sample_ = 0;
        speech_detected_ = false;
        samples_since_speech_ = 0;
        if (vad_ != nullptr) {
            vad_->Reset();
        }
    }

    void Feed(const float* samples, std::size_t count) {
        pending_.insert(pending_.end(), samples, samples + count);
        while (pending_.size() >= kChunkSamples) {
            std::vector<float> chunk(pending_.begin(), pending_.begin() + kChunkSamples);
            pending_.erase(pending_.begin(), pending_.begin() + kChunkSamples);
            const std::size_t chunk_start = total_samples_;
            total_samples_ += kChunkSamples;
            const bool speech = IsChunkSpeech(chunk.data(), kChunkSamples);
            if (speech) {
                if (!speech_detected_) {
                    first_speech_sample_ = chunk_start;
                }
                last_speech_sample_ = chunk_start + kChunkSamples;
                speech_detected_ = true;
                samples_since_speech_ = 0;
            } else {
                samples_since_speech_ += kChunkSamples;
            }
        }
    }

    bool SpeechDetected() const {
        return speech_detected_;
    }

    std::size_t TotalSamples() const {
        return total_samples_;
    }

    std::chrono::milliseconds SilenceMs(std::uint32_t sample_rate) const {
        return std::chrono::milliseconds(samples_since_speech_ * 1000 / sample_rate);
    }

    std::chrono::milliseconds DurationMs(std::uint32_t sample_rate) const {
        return std::chrono::milliseconds(total_samples_ * 1000 / sample_rate);
    }

    std::pair<std::size_t, std::size_t> SpeechInterval(std::size_t total_recorded) const {
        if (!speech_detected_) {
            return {0, 0};
        }
        const std::size_t begin = first_speech_sample_ > kHeadPadSamples
                                      ? first_speech_sample_ - kHeadPadSamples
                                      : 0;
        const std::size_t end = last_speech_sample_ + kTailPadSamples > total_recorded
                                    ? total_recorded
                                    : last_speech_sample_ + kTailPadSamples;
        return {begin, end};
    }

private:
    bool IsChunkSpeech(const float* chunk, std::size_t count) {
        if (vad_ != nullptr) {
            return vad_->IsSpeech(std::vector<float>(chunk, chunk + count));
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (chunk[i] > 0.0001f || chunk[i] < -0.0001f) {
                return true;
            }
        }
        return false;
    }

    IVadProvider* vad_ = nullptr;
    std::vector<float> pending_;
    std::size_t total_samples_ = 0;
    std::size_t first_speech_sample_ = 0;
    std::size_t last_speech_sample_ = 0;
    std::size_t samples_since_speech_ = 0;
    bool speech_detected_ = false;
};
