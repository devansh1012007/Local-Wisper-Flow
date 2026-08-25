#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

enum class State {
    IDLE,
    ARMED,
    CAPTURING,
    TRANSCRIBING,
    POSTPROCESSING,
    EMITTING,
};

enum class Trigger {
    HOTKEY_PRESS,
    HOTKEY_RELEASE,
    VAD_SPEECH,
    VAD_SILENCE,
    SST_COMPLETE,
    SST_FAILED,
    TIMEOUT,
};

class DictationFSM {
public:
    using TransitionListener = std::function<void(State previous, Trigger trigger, State current)>;

    void SetTransitionListener(TransitionListener listener);

    bool TriggerEvent(Trigger trigger, const std::unordered_map<std::string, std::any>& context = {});
    State GetCurrentState();
    State CurrentState() const noexcept;

private:
    std::mutex mutex_;
    State state_ = State::IDLE;
    std::atomic<State> state_snapshot_{State::IDLE};
    TransitionListener listener_;
};

const char* ToString(State state);
const char* ToString(Trigger trigger);
