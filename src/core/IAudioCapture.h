#pragma once

#include <vector>

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual std::vector<float> GetRecordedAudio() = 0;
};
