#include "core/DictationFSM.h"

namespace {

struct Transition {
    State source;
    Trigger trigger;
    State target;
};

constexpr Transition kTransitionTable[] = {
    {State::IDLE,           Trigger::HOTKEY_PRESS,   State::ARMED},

    {State::ARMED,          Trigger::HOTKEY_RELEASE, State::IDLE},
    {State::ARMED,          Trigger::VAD_SPEECH,     State::CAPTURING},
    {State::ARMED,          Trigger::TIMEOUT,        State::IDLE},

    {State::CAPTURING,      Trigger::HOTKEY_RELEASE, State::TRANSCRIBING},
    {State::CAPTURING,      Trigger::VAD_SPEECH,     State::CAPTURING},
    {State::CAPTURING,      Trigger::VAD_SILENCE,    State::TRANSCRIBING},
    {State::CAPTURING,      Trigger::TIMEOUT,        State::TRANSCRIBING},

    {State::TRANSCRIBING,   Trigger::SST_COMPLETE,   State::POSTPROCESSING},
    {State::TRANSCRIBING,   Trigger::SST_FAILED,     State::IDLE},
    {State::TRANSCRIBING,   Trigger::TIMEOUT,        State::IDLE},

    {State::POSTPROCESSING, Trigger::SST_COMPLETE,   State::EMITTING},
    {State::POSTPROCESSING, Trigger::SST_FAILED,     State::EMITTING},
    {State::POSTPROCESSING, Trigger::TIMEOUT,        State::EMITTING},

    {State::EMITTING,       Trigger::SST_COMPLETE,   State::IDLE},
    {State::EMITTING,       Trigger::SST_FAILED,     State::IDLE},
    {State::EMITTING,       Trigger::TIMEOUT,        State::IDLE},
};

const Transition* FindTransition(State source, Trigger trigger) {
    for (const Transition& transition : kTransitionTable) {
        if (transition.source == source && transition.trigger == trigger) {
            return &transition;
        }
    }
    return nullptr;
}

}  // namespace

void DictationFSM::SetTransitionListener(TransitionListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = std::move(listener);
}

bool DictationFSM::TriggerEvent(Trigger trigger,
                                const std::unordered_map<std::string, std::any>& context) {
    TransitionListener listener;
    State previous;
    State current;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const Transition* transition = FindTransition(state_, trigger);
        if (transition == nullptr) {
            return false;
        }
        previous = state_;
        state_ = transition->target;
        current = transition->target;
        state_snapshot_.store(current, std::memory_order_relaxed);
        listener = listener_;
    }

    (void)context;
    if (listener) {
        listener(previous, trigger, current);
    }

    return true;
}

State DictationFSM::GetCurrentState() {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

State DictationFSM::CurrentState() const noexcept {
    return state_snapshot_.load(std::memory_order_relaxed);
}

const char* ToString(State state) {
    switch (state) {
        case State::IDLE:
            return "IDLE";
        case State::ARMED:
            return "ARMED";
        case State::CAPTURING:
            return "CAPTURING";
        case State::TRANSCRIBING:
            return "TRANSCRIBING";
        case State::POSTPROCESSING:
            return "POSTPROCESSING";
        case State::EMITTING:
            return "EMITTING";
    }
    return "UNKNOWN";
}

const char* ToString(Trigger trigger) {
    switch (trigger) {
        case Trigger::HOTKEY_PRESS:
            return "HOTKEY_PRESS";
        case Trigger::HOTKEY_RELEASE:
            return "HOTKEY_RELEASE";
        case Trigger::VAD_SPEECH:
            return "VAD_SPEECH";
        case Trigger::VAD_SILENCE:
            return "VAD_SILENCE";
        case Trigger::SST_COMPLETE:
            return "SST_COMPLETE";
        case Trigger::SST_FAILED:
            return "SST_FAILED";
        case Trigger::TIMEOUT:
            return "TIMEOUT";
    }
    return "UNKNOWN";
}
