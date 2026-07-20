from vision_sst.state_machine import DictationFSM, State, Trigger


def test_state_machine_transitions_with_guard():
    fsm = DictationFSM()
    transitions = []

    def record(old_state, trigger, new_state):
        transitions.append((old_state, trigger, new_state))

    fsm.on_transition(record)

    assert fsm.trigger(Trigger.HOTKEY_PRESS)
    assert fsm.state is State.ARMED

    assert fsm.trigger(Trigger.VAD_SPEECH)
    assert fsm.state is State.CAPTURING

    assert fsm.trigger(Trigger.VAD_SILENCE)
    assert fsm.state is State.TRANSCRIBING

    assert fsm.trigger(Trigger.SST_COMPLETE)
    assert fsm.state is State.POSTPROCESSING

    assert fsm.trigger(Trigger.LLM_COMPLETE)
    assert fsm.state is State.EMITTING

    assert fsm.trigger(Trigger.TIMEOUT)
    assert fsm.state is State.IDLE

    assert transitions
    assert transitions[-1][2] is State.IDLE


def test_state_machine_rejects_invalid_transition():
    fsm = DictationFSM()

    assert fsm.trigger(Trigger.VAD_SPEECH) is False
    assert fsm.state is State.IDLE
