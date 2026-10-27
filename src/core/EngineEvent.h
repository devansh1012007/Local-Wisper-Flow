#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

enum class EngineEventType {
    PttPress,
    PttRelease,
    Cancel,
    Quit,
};

struct EngineEvent {
    EngineEventType type;
    std::chrono::steady_clock::time_point timestamp;
};

class EngineEventQueue {
public:
    void Push(EngineEvent event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                return;
            }
            events_.push_back(event);
        }
        condition_.notify_one();
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            events_.clear();
        }
        condition_.notify_all();
    }

    std::optional<EngineEvent> PopFor(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condition_.wait_for(lock, timeout, [this] { return !events_.empty() || closed_; })) {
            return std::nullopt;
        }
        if (events_.empty()) {
            return std::nullopt;
        }
        EngineEvent event = events_.front();
        events_.pop_front();
        return event;
    }

    void Drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<EngineEvent> events_;
    bool closed_ = false;
};
