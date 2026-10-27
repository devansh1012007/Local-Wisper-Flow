#pragma once

#include <cstddef>
#include <vector>

class PreRollBuffer {
public:
    explicit PreRollBuffer(std::size_t capacity_samples)
        : buffer_(capacity_samples, 0.0f), capacity_(capacity_samples) {}

    void Write(const float* samples, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            buffer_[pos_] = samples[i];
            pos_ = (pos_ + 1) % capacity_;
            if (count_ < capacity_) {
                ++count_;
            }
        }
    }

    std::vector<float> Snapshot() const {
        std::vector<float> out;
        out.reserve(count_);
        const std::size_t start = count_ < capacity_ ? 0 : pos_;
        for (std::size_t i = 0; i < count_; ++i) {
            out.push_back(buffer_[(start + i) % capacity_]);
        }
        return out;
    }

    void Clear() {
        pos_ = 0;
        count_ = 0;
    }

    std::size_t Size() const {
        return count_;
    }

    std::size_t Capacity() const {
        return capacity_;
    }

private:
    std::vector<float> buffer_;
    std::size_t capacity_;
    std::size_t pos_ = 0;
    std::size_t count_ = 0;
};
