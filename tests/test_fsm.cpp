#include "core/DictationFSM.h"

#include <doctest/doctest.h>
#include <string>
#include <unordered_map>
#include <vector>

TEST_CASE("fsm walks the full dictation cycle and returns to idle") {
    DictationFSM fsm;
    int transitions = 0;
    fsm.SetTransitionListener([&](State, Trigger, State) { ++transitions; });

    CHECK(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
    CHECK(fsm.TriggerEvent(Trigger::VAD_SPEECH));
    CHECK(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
    CHECK(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    CHECK(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    CHECK(fsm.TriggerEvent(Trigger::SST_COMPLETE));

    CHECK(transitions == 6);
    CHECK(fsm.GetCurrentState() == State::IDLE);
    CHECK(fsm.CurrentState() == State::IDLE);
}

TEST_CASE("fsm rejects transitions outside the table") {
    DictationFSM fsm;
    CHECK_FALSE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
    CHECK_FALSE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    CHECK_FALSE(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
    CHECK(fsm.GetCurrentState() == State::IDLE);

    fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
    CHECK(fsm.GetCurrentState() == State::ARMED);
    CHECK_FALSE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
    CHECK_FALSE(fsm.TriggerEvent(Trigger::VAD_SILENCE));
}

TEST_CASE("fsm listener sees previous and current states") {
    DictationFSM fsm;
    State seen_previous = State::IDLE;
    State seen_current = State::IDLE;
    Trigger seen_trigger = Trigger::TIMEOUT;
    fsm.SetTransitionListener([&](State previous, Trigger trigger, State current) {
        seen_previous = previous;
        seen_trigger = trigger;
        seen_current = current;
    });

    fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
    CHECK(seen_previous == State::IDLE);
    CHECK(seen_current == State::ARMED);
    CHECK(seen_trigger == Trigger::HOTKEY_PRESS);
}

TEST_CASE("fsm state names round trip") {
    CHECK(std::string(ToString(State::CAPTURING)) == "CAPTURING");
    CHECK(std::string(ToString(Trigger::HOTKEY_RELEASE)) == "HOTKEY_RELEASE");
}

TEST_CASE("exhaustive state-trigger table rejects every unlisted pair") {
    struct Row {
        State source;
        Trigger trigger;
        bool accepted;
    };

    const std::vector<Row> table = {
        {State::IDLE, Trigger::HOTKEY_PRESS, true},
        {State::IDLE, Trigger::HOTKEY_RELEASE, false},
        {State::IDLE, Trigger::VAD_SPEECH, false},
        {State::IDLE, Trigger::VAD_SILENCE, false},
        {State::IDLE, Trigger::SST_COMPLETE, false},
        {State::IDLE, Trigger::SST_FAILED, false},
        {State::IDLE, Trigger::TIMEOUT, false},
        {State::IDLE, Trigger::CANCEL, false},

        {State::ARMED, Trigger::HOTKEY_PRESS, false},
        {State::ARMED, Trigger::HOTKEY_RELEASE, true},
        {State::ARMED, Trigger::VAD_SPEECH, true},
        {State::ARMED, Trigger::VAD_SILENCE, false},
        {State::ARMED, Trigger::SST_COMPLETE, false},
        {State::ARMED, Trigger::SST_FAILED, false},
        {State::ARMED, Trigger::TIMEOUT, true},
        {State::ARMED, Trigger::CANCEL, true},

        {State::CAPTURING, Trigger::HOTKEY_PRESS, false},
        {State::CAPTURING, Trigger::HOTKEY_RELEASE, true},
        {State::CAPTURING, Trigger::VAD_SPEECH, true},
        {State::CAPTURING, Trigger::VAD_SILENCE, true},
        {State::CAPTURING, Trigger::SST_COMPLETE, false},
        {State::CAPTURING, Trigger::SST_FAILED, false},
        {State::CAPTURING, Trigger::TIMEOUT, true},
        {State::CAPTURING, Trigger::CANCEL, true},

        {State::TRANSCRIBING, Trigger::HOTKEY_PRESS, false},
        {State::TRANSCRIBING, Trigger::HOTKEY_RELEASE, false},
        {State::TRANSCRIBING, Trigger::VAD_SPEECH, false},
        {State::TRANSCRIBING, Trigger::VAD_SILENCE, false},
        {State::TRANSCRIBING, Trigger::SST_COMPLETE, true},
        {State::TRANSCRIBING, Trigger::SST_FAILED, true},
        {State::TRANSCRIBING, Trigger::TIMEOUT, true},
        {State::TRANSCRIBING, Trigger::CANCEL, true},

        {State::POSTPROCESSING, Trigger::HOTKEY_PRESS, false},
        {State::POSTPROCESSING, Trigger::HOTKEY_RELEASE, false},
        {State::POSTPROCESSING, Trigger::VAD_SPEECH, false},
        {State::POSTPROCESSING, Trigger::VAD_SILENCE, false},
        {State::POSTPROCESSING, Trigger::SST_COMPLETE, true},
        {State::POSTPROCESSING, Trigger::SST_FAILED, true},
        {State::POSTPROCESSING, Trigger::TIMEOUT, true},
        {State::POSTPROCESSING, Trigger::CANCEL, false},

        {State::EMITTING, Trigger::HOTKEY_PRESS, false},
        {State::EMITTING, Trigger::HOTKEY_RELEASE, false},
        {State::EMITTING, Trigger::VAD_SPEECH, false},
        {State::EMITTING, Trigger::VAD_SILENCE, false},
        {State::EMITTING, Trigger::SST_COMPLETE, true},
        {State::EMITTING, Trigger::SST_FAILED, true},
        {State::EMITTING, Trigger::TIMEOUT, true},
        {State::EMITTING, Trigger::CANCEL, false},
    };

    for (const Row& row : table) {
        DictationFSM fsm;
        switch (row.source) {
            case State::IDLE:
                break;
            case State::ARMED:
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
                break;
            case State::CAPTURING:
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
                REQUIRE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
                break;
            case State::TRANSCRIBING:
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
                REQUIRE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
                break;
            case State::POSTPROCESSING:
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
                REQUIRE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
                REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
                break;
            case State::EMITTING:
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_PRESS));
                REQUIRE(fsm.TriggerEvent(Trigger::VAD_SPEECH));
                REQUIRE(fsm.TriggerEvent(Trigger::HOTKEY_RELEASE));
                REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
                REQUIRE(fsm.TriggerEvent(Trigger::SST_COMPLETE));
                break;
        }
        INFO("state=", ToString(row.source), " trigger=", ToString(row.trigger));
        CHECK(fsm.GetCurrentState() == row.source);
        const bool accepted = fsm.TriggerEvent(row.trigger);
        CHECK(accepted == row.accepted);
    }
}
