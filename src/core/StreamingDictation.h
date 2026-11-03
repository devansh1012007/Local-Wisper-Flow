#pragma once

#include "core/IInferenceProvider.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class StreamingDictator {
public:
    explicit StreamingDictator(IInferenceProvider& stt);
    ~StreamingDictator();

    StreamingDictator(const StreamingDictator&) = delete;
    StreamingDictator& operator=(const StreamingDictator&) = delete;

    void Begin();
    void CommitSegment(std::vector<float> pcm);
    bool Finish(const std::vector<float>& tail_pcm, std::string& out_text);
    void Cancel();

    std::size_t CommittedCount() const;
    std::size_t PendingCount() const;

    static std::string JoinTexts(const std::string& a, const std::string& b);
    static std::string TruncateContext(const std::string& text, std::size_t max_chars = 200);

private:
    void Loop();

    IInferenceProvider& stt_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::vector<float>> queue_;
    std::string partial_;
    std::string context_;
    bool busy_ = false;
    bool failed_ = false;
    bool canceled_ = false;
    bool stopped_ = false;
    std::uint64_t generation_ = 0;
    std::size_t committed_ = 0;
};
