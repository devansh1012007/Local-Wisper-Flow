#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

template <typename T>
class EventQueue {
public:
    void Push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(std::move(value));
        }
        condition_.notify_one();
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        condition_.notify_all();
    }

    bool Closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::optional<T> PopFor(int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                 [this] { return !events_.empty() || closed_; })) {
            return std::nullopt;
        }
        if (events_.empty()) {
            return std::nullopt;
        }
        T value = std::move(events_.front());
        events_.pop_front();
        return value;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<T> events_;
    bool closed_ = false;
};
