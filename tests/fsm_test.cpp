#include <doctest/doctest.h>

#include "core/DictationFSM.h"

#include <iostream>
#include <tuple>
#include <vector>

namespace {

void DriveTo(DictationFSM& fsm, State state) {
    switch (state) {
        case State::IDLE:
            break;
        case State::ARMED:
            fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
            break;
        case State::CAPTURING:
            fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
            fsm.TriggerEvent(Trigger::VAD_SPEECH);
            break;
        case State::TRANSCRIBING:
            fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
            fsm.TriggerEvent(Trigger::VAD_SPEECH);
            fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);
            break;
        case State::POSTPROCESSING:
            fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
            fsm.TriggerEvent(Trigger::VAD_SPEECH);
            fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);
            fsm.TriggerEvent(Trigger::SST_COMPLETE);
            break;
        case State::EMITTING:
            fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
            fsm.TriggerEvent(Trigger::VAD_SPEECH);
            fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);
            fsm.TriggerEvent(Trigger::SST_COMPLETE);
            fsm.TriggerEvent(Trigger::SST_COMPLETE);
            break;
    }
}

}  // namespace

TEST_CASE("fsm walks a full dictation cycle") {
    DictationFSM fsm;
    fsm.SetTransitionListener([](State previous, Trigger trigger, State current) {
        std::cout << "[fsm] " << ToString(previous) << " -> " << ToString(current) << " ["
                  << ToString(trigger) << "]" << std::endl;
    });

    REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
    REQUIRE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
    REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
    REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    CHECK(fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("fsm rejects invalid transitions") {
    DictationFSM fsm;
    CHECK_FALSE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
    CHECK(fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("fsm transition table is exhaustive and closed") {
    const std::vector<State> states = {
        State::IDLE,      State::ARMED,  State::CAPTURING, State::TRANSCRIBING,
        State::POSTPROCESSING, State::EMITTING,
    };
    const std::vector<Trigger> triggers = {
        Trigger::HOTKEY_PRESS, Trigger::HOTKEY_RELEASE, Trigger::VAD_SPEECH,
        Trigger::VAD_SILENCE,  Trigger::SST_COMPLETE,   Trigger::SST_FAILED,
        Trigger::TIMEOUT,
    };
    const std::vector<std::tuple<State, Trigger, State>> expected = {
        {State::IDLE, Trigger::HOTKEY_PRESS, State::ARMED},
        {State::ARMED, Trigger::HOTKEY_RELEASE, State::IDLE},
        {State::ARMED, Trigger::VAD_SPEECH, State::CAPTURING},
        {State::ARMED, Trigger::TIMEOUT, State::IDLE},
        {State::CAPTURING, Trigger::HOTKEY_RELEASE, State::TRANSCRIBING},
        {State::CAPTURING, Trigger::VAD_SPEECH, State::CAPTURING},
        {State::CAPTURING, Trigger::VAD_SILENCE, State::TRANSCRIBING},
        {State::CAPTURING, Trigger::TIMEOUT, State::TRANSCRIBING},
        {State::TRANSCRIBING, Trigger::SST_COMPLETE, State::POSTPROCESSING},
        {State::TRANSCRIBING, Trigger::SST_FAILED, State::IDLE},
        {State::TRANSCRIBING, Trigger::TIMEOUT, State::IDLE},
        {State::POSTPROCESSING, Trigger::SST_COMPLETE, State::EMITTING},
        {State::POSTPROCESSING, Trigger::SST_FAILED, State::EMITTING},
        {State::POSTPROCESSING, Trigger::TIMEOUT, State::EMITTING},
        {State::EMITTING, Trigger::SST_COMPLETE, State::IDLE},
        {State::EMITTING, Trigger::SST_FAILED, State::IDLE},
        {State::EMITTING, Trigger::TIMEOUT, State::IDLE},
    };

    for (const State state : states) {
        for (const Trigger trigger : triggers) {
            bool found = false;
            State target{};
            for (const auto& rule : expected) {
                if (std::get<0>(rule) == state && std::get<1>(rule) == trigger) {
                    found = true;
                    target = std::get<2>(rule);
                    break;
                }
            }
            DictationFSM fsm;
            DriveTo(fsm, state);
            REQUIRE(fsm.GetCurrentState() == state);
            const bool accepted = fsm.TriggerEvent(trigger);
            if (found) {
                const std::string label =
                    std::string(ToString(state)) + " + " + ToString(trigger) + " should be accepted";
                CHECK_MESSAGE(accepted, label.c_str());
                CHECK(fsm.GetCurrentState() == target);
            } else {
                const std::string label =
                    std::string(ToString(state)) + " + " + ToString(trigger) + " must be rejected";
                CHECK_MESSAGE(!accepted, label.c_str());
            }
        }
    }
}
