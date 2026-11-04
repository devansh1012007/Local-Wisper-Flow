#pragma once

#include <cstddef>
#include <vector>

class IAudioChunkSink {
public:
    virtual ~IAudioChunkSink() = default;
    virtual void OnAudioChunk(const float* samples, std::size_t frame_count) = 0;
};

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual std::vector<float> GetRecordedAudio() = 0;

    virtual void SetChunkSink(IAudioChunkSink* sink) { (void)sink; }
    virtual float AudioLevel() const { return 0.0f; }
    virtual int SampleRate() const { return 16000; }
    virtual bool DeviceLost() const { return false; }
};
