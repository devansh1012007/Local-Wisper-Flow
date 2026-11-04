#pragma once

#include <cstdint>

enum class EngineEventType {
    PttPress,
    PttRelease,
    Toggle,
    Cancel,
    CommandMode,
    PasteLast,
    Quit,
};

struct EngineEvent {
    EngineEventType type;
    std::int64_t timestamp_ms = 0;
};

struct EngineSettings {
    int min_hold_ms = 250;
    int max_recording_ms = 30000;
    int hands_free_silence_ms = 1500;
    int min_tap_samples = 1600;
    bool quiet_speech_boost = false;
    bool sound_enabled = true;
    double sound_volume = 0.3;
    bool streaming_stt = true;
    int streaming_min_segment_ms = 3000;
    int streaming_pause_ms = 700;
};
