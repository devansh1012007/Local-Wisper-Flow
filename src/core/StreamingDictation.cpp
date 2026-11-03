#include "core/StreamingDictation.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace {

std::string TrimCopy(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool OpensWithPunctuation(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(text[0]);
    switch (first) {
        case ',':
        case '.':
        case '!':
        case '?':
        case ';':
        case ':':
        case ')':
        case ']':
        case '}':
            return true;
        default:
            return false;
    }
}

}

StreamingDictator::StreamingDictator(IInferenceProvider& stt) : stt_(stt) {
    thread_ = std::thread([this] { Loop(); });
}

StreamingDictator::~StreamingDictator() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        canceled_ = true;
        queue_.clear();
    }
    ready_.notify_all();
    stt_.RequestCancel();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void StreamingDictator::Begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    partial_.clear();
    context_.clear();
    busy_ = false;
    failed_ = false;
    canceled_ = false;
    ++generation_;
    committed_ = 0;
}

void StreamingDictator::CommitSegment(std::vector<float> pcm) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (canceled_ || stopped_) {
            return;
        }
        queue_.push_back(std::move(pcm));
        ++committed_;
    }
    ready_.notify_one();
}

bool StreamingDictator::Finish(const std::vector<float>& tail_pcm, std::string& out_text) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] { return queue_.empty() && !busy_; });
        if (canceled_ || failed_) {
            return false;
        }
        out_text = partial_;
    }
    if (!tail_pcm.empty()) {
        std::string tail_text;
        try {
            tail_text = stt_.TranscribeWithPrompt(tail_pcm, context_);
        } catch (const std::exception&) {
            return false;
        }
        out_text = JoinTexts(out_text, tail_text);
    }
    return true;
}

void StreamingDictator::Cancel() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        canceled_ = true;
    }
    ready_.notify_all();
    stt_.RequestCancel();
}

std::size_t StreamingDictator::CommittedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return committed_;
}

std::size_t StreamingDictator::PendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() + (busy_ ? 1 : 0);
}

std::string StreamingDictator::JoinTexts(const std::string& a, const std::string& b) {
    const std::string left = TrimCopy(a);
    const std::string right = TrimCopy(b);
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (OpensWithPunctuation(right)) {
        return left + right;
    }
    return left + " " + right;
}

std::string StreamingDictator::TruncateContext(const std::string& text, std::size_t max_chars) {
    const std::string trimmed = TrimCopy(text);
    if (trimmed.size() <= max_chars) {
        return trimmed;
    }
    std::string suffix = trimmed.substr(trimmed.size() - max_chars);
    const std::size_t space = suffix.find(' ');
    if (space != std::string::npos) {
        suffix = suffix.substr(space + 1);
    }
    return suffix;
}

void StreamingDictator::Loop() {
    for (;;) {
        std::vector<float> segment;
        std::string prompt;
        std::uint64_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return !queue_.empty() || stopped_; });
            if (queue_.empty()) {
                if (stopped_) {
                    return;
                }
                continue;
            }
            segment = std::move(queue_.front());
            queue_.pop_front();
            prompt = context_;
            generation = generation_;
            busy_ = true;
        }
        bool ok = false;
        std::string text;
        try {
            text = stt_.TranscribeWithPrompt(segment, prompt);
            ok = true;
        } catch (const std::exception&) {
            ok = false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            busy_ = false;
            if (!canceled_ && generation == generation_) {
                if (ok) {
                    partial_ = JoinTexts(partial_, text);
                    context_ = TruncateContext(text);
                } else {
                    failed_ = true;
                }
            }
        }
        ready_.notify_all();
    }
}
