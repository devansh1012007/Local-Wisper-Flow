from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional


class State(Enum):
    IDLE = auto()
    ARMED = auto()
    CAPTURING = auto()
    TRANSCRIBING = auto()
    POSTPROCESSING = auto()
    EMITTING = auto()


class Trigger(Enum):
    HOTKEY_PRESS = auto()
    HOTKEY_RELEASE = auto()
    VAD_SPEECH = auto()
    VAD_SILENCE = auto()
    SST_COMPLETE = auto()
    SST_FAILED = auto()
    LLM_COMPLETE = auto()
    TIMEOUT = auto()
    ABORT = auto()


@dataclass(frozen=True)
class Transition:
    from_state: State
    trigger: Trigger
    to_state: State
    guard: Optional[Callable[[dict], bool]] = None
    action: Optional[Callable[[dict], None]] = None


class DictationFSM:
    TRANSITIONS = [
        Transition(State.IDLE, Trigger.HOTKEY_PRESS, State.ARMED),
        Transition(State.ARMED, Trigger.VAD_SPEECH, State.CAPTURING),
        Transition(State.ARMED, Trigger.HOTKEY_RELEASE, State.IDLE),
        Transition(State.CAPTURING, Trigger.VAD_SILENCE, State.TRANSCRIBING),
        Transition(State.CAPTURING, Trigger.HOTKEY_RELEASE, State.TRANSCRIBING),
        Transition(State.CAPTURING, Trigger.TIMEOUT, State.TRANSCRIBING),
        Transition(State.TRANSCRIBING, Trigger.SST_COMPLETE, State.POSTPROCESSING),
        Transition(State.TRANSCRIBING, Trigger.SST_FAILED, State.IDLE),
        Transition(State.POSTPROCESSING, Trigger.LLM_COMPLETE, State.EMITTING),
        Transition(State.EMITTING, Trigger.HOTKEY_RELEASE, State.IDLE),
        Transition(State.EMITTING, Trigger.TIMEOUT, State.IDLE),
        Transition(State.ARMED, Trigger.ABORT, State.IDLE),
        Transition(State.CAPTURING, Trigger.ABORT, State.IDLE),
        Transition(State.TRANSCRIBING, Trigger.ABORT, State.IDLE),
        Transition(State.POSTPROCESSING, Trigger.ABORT, State.IDLE),
    ]

    def __init__(self):
        self.state = State.IDLE
        self._on_transition: Optional[Callable[[State, Trigger, State], None]] = None

    def on_transition(self, callback: Callable[[State, Trigger, State], None]):
        self._on_transition = callback

    def trigger(self, event: Trigger, context: Optional[dict] = None) -> bool:
        for transition in self.TRANSITIONS:
            if transition.from_state != self.state or transition.trigger != event:
                continue
            if transition.guard and not transition.guard(context or {}):
                continue
            old_state = self.state
            self.state = transition.to_state
            if transition.action:
                transition.action(context or {})
            if self._on_transition:
                self._on_transition(old_state, event, self.state)
            return True
        return False
