#include "core/HotkeyTracker.h"

#include <algorithm>
#include <utility>

void HotkeyTracker::SetBindings(std::vector<Binding> bindings) {
    bindings_ = std::move(bindings);
    chords_.clear();
    chords_.resize(bindings_.size());
}

void HotkeyTracker::SetOptions(const Options& options) {
    options_ = options;
}

void HotkeyTracker::SetCancelArmed(bool armed) {
    cancel_armed_ = armed;
}

EngineEvent HotkeyTracker::MakeEvent(EngineEventType type, std::int64_t timestamp_ms) {
    EngineEvent event;
    event.type = type;
    event.timestamp_ms = timestamp_ms;
    return event;
}

void HotkeyTracker::ResetChord(ChordState& chord) {
    chord.in_progress = false;
    chord.fully_matched = false;
    chord.down_keys.clear();
    chord.swallowed_keys.clear();
}

EngineEventType HotkeyTracker::EventForRole(BindingRole role, std::int64_t now_ms) {
    switch (role) {
        case BindingRole::HandsFreeToggle:
        case BindingRole::PushToTalk:
            break;
        case BindingRole::CommandMode:
            return EngineEventType::CommandMode;
        case BindingRole::PasteLast:
            return EngineEventType::PasteLast;
        case BindingRole::Cancel:
            return EngineEventType::Cancel;
    }
    (void)now_ms;
    return EngineEventType::PttPress;
}

TrackerResult HotkeyTracker::Feed(const RawKeyEvent& event) {
    TrackerResult result;

    if (event.injected) {
        result.swallow = false;
        return result;
    }

    if (event.vk == HotkeyBinding::kCtrl) {
        ctrl_down_ = event.is_down;
    }

    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        const Binding& binding = bindings_[i];
        if (binding.keys.size() == 1 && binding.keys[0] == HotkeyBinding::kRightAlt &&
            binding.role == BindingRole::PushToTalk && options_.avoid_altgr_conflict &&
            event.is_down && event.vk == HotkeyBinding::kRightAlt && ctrl_down_) {
            result.swallow = false;
            return result;
        }
    }

    bool any_swallow = false;
    bool any_match = false;

    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        const Binding& binding = bindings_[i];
        if (std::find(binding.keys.begin(), binding.keys.end(), event.vk) ==
            binding.keys.end()) {
            continue;
        }
        any_match = true;
        ChordState& chord = chords_[i];

        if (binding.role == BindingRole::Cancel) {
            if (cancel_armed_) {
                any_swallow = true;
                if (event.is_down) {
                    result.events.push_back(
                        MakeEvent(EngineEventType::Cancel, event.timestamp_ms));
                }
            }
            continue;
        }

        if (event.is_down) {
            if (chord.down_keys.count(event.vk) > 0) {
                any_swallow = any_swallow || chord.swallowed_keys.count(event.vk) > 0;
                continue;
            }
            if (!chord.in_progress) {
                chord.in_progress = true;
                chord.started_ms = event.timestamp_ms;
                chord.down_keys.insert(event.vk);
                chord.fully_matched = HotkeyBinding::IsSatisfied(binding, chord.down_keys);
                if (chord.fully_matched) {
                    any_swallow = true;
                    chord.swallowed_keys.insert(event.vk);
                    EngineEventType type = EventForRole(binding.role, event.timestamp_ms);
                    if (binding.role == BindingRole::PushToTalk) {
                        if (awaiting_double_tap_ &&
                            event.timestamp_ms - last_press_ms_ <=
                                static_cast<std::int64_t>(options_.double_tap_window_ms)) {
                            type = EngineEventType::Toggle;
                            awaiting_double_tap_ = false;
                        } else {
                            awaiting_double_tap_ = true;
                        }
                        last_press_ms_ = event.timestamp_ms;
                    }
                    result.events.push_back(MakeEvent(type, event.timestamp_ms));
                }
                continue;
            }
            chord.down_keys.insert(event.vk);
            chord.fully_matched = HotkeyBinding::IsSatisfied(binding, chord.down_keys);
            any_swallow = true;
            chord.swallowed_keys.insert(event.vk);
            if (chord.fully_matched) {
                EngineEventType type = EventForRole(binding.role, event.timestamp_ms);
                if (binding.role == BindingRole::PushToTalk) {
                    if (awaiting_double_tap_ &&
                        event.timestamp_ms - last_press_ms_ <=
                            static_cast<std::int64_t>(options_.double_tap_window_ms)) {
                        type = EngineEventType::Toggle;
                        awaiting_double_tap_ = false;
                    } else {
                        awaiting_double_tap_ = true;
                    }
                    last_press_ms_ = event.timestamp_ms;
                }
                result.events.push_back(MakeEvent(type, event.timestamp_ms));
            }
            continue;
        }

        const bool was_swallowed = chord.swallowed_keys.count(event.vk) > 0;
        chord.down_keys.erase(event.vk);
        chord.swallowed_keys.erase(event.vk);
        if (chord.fully_matched) {
            any_swallow = true;
            if (binding.role == BindingRole::PushToTalk) {
                result.events.push_back(
                    MakeEvent(EngineEventType::PttRelease, event.timestamp_ms));
            }
            ResetChord(chord);
            continue;
        }
        if (was_swallowed) {
            any_swallow = true;
        }
        if (chord.in_progress && chord.down_keys.empty()) {
            ResetChord(chord);
        }
    }

    result.swallow = any_match && any_swallow;
    return result;
}

TrackerResult HotkeyTracker::Tick(std::int64_t now_ms) {
    TrackerResult result;
    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        ChordState& chord = chords_[i];
        if (chord.in_progress &&
            now_ms - chord.started_ms >
                static_cast<std::int64_t>(options_.stuck_key_guard_ms)) {
            const Binding& binding = bindings_[i];
            if (binding.role == BindingRole::PushToTalk && chord.fully_matched) {
                result.events.push_back(MakeEvent(EngineEventType::PttRelease, now_ms));
            }
            ResetChord(chord);
        }
    }
    return result;
}
