#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

class AudioPipe {
public:
    void SetEnabled(bool enabled) {
        enabled_.store(enabled, std::memory_order_relaxed);
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
    }

    void Write(const float* samples, std::size_t count) {
        if (!enabled_.load(std::memory_order_relaxed) || count == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.insert(buffer_.end(), samples, samples + count);
    }

    std::vector<float> ReadAll() {
        std::vector<float> out;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            out.swap(buffer_);
        }
        return out;
    }

private:
    std::atomic<bool> enabled_{false};
    std::mutex mutex_;
    std::vector<float> buffer_;
};
