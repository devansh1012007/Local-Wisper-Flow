#pragma once

#include <cstddef>
#include <functional>
#include <vector>

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual std::vector<float> GetRecordedAudio() = 0;

    virtual void SetChunkSink(std::function<void(const float*, std::size_t)> sink) {
        (void)sink;
    }

    virtual float AudioLevel() const noexcept {
        return 0.0f;
    }

    virtual bool DeviceLost() const noexcept {
        return false;
    }
};
