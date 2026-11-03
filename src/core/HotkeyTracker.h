#pragma once

#include "core/EngineTypes.h"
#include "core/HotkeyBinding.h"

#include <cstdint>
#include <set>
#include <vector>

struct RawKeyEvent {
    std::uint32_t vk = 0;
    bool is_down = false;
    bool injected = false;
    std::int64_t timestamp_ms = 0;
};

struct TrackerResult {
    std::vector<EngineEvent> events;
    bool swallow = false;
};

class HotkeyTracker {
public:
    struct Options {
        int double_tap_window_ms = 400;
        int stuck_key_guard_ms = 35000;
        bool avoid_altgr_conflict = true;
    };

    void SetBindings(std::vector<Binding> bindings);
    void SetOptions(const Options& options);
    void SetCancelArmed(bool armed);

    TrackerResult Feed(const RawKeyEvent& event);
    TrackerResult Tick(std::int64_t now_ms);

private:
    struct ChordState {
        bool in_progress = false;
        bool fully_matched = false;
        std::set<std::uint32_t> down_keys;
        std::set<std::uint32_t> swallowed_keys;
        std::int64_t started_ms = 0;
    };

    ChordState* FindChord(std::uint32_t vk);
    void ResetChord(ChordState& chord);
    static EngineEvent MakeEvent(EngineEventType type, std::int64_t timestamp_ms);
    static EngineEventType EventForRole(BindingRole role, std::int64_t now_ms);

    std::vector<Binding> bindings_;
    std::vector<ChordState> chords_;
    Options options_;
    bool cancel_armed_ = false;
    bool ctrl_down_ = false;
    std::int64_t last_press_ms_ = 0;
    bool awaiting_double_tap_ = false;
};
