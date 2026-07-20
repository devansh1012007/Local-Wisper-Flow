from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Literal
import uuid

EventType = Literal[
    "audio:capture_started",
    "audio:capture_stopped",
    "audio:level",
    "vad:speech_started",
    "vad:speech_ended",
    "vad:utterance_ready",
    "sst:transcription_started",
    "sst:transcription_complete",
    "sst:transcription_failed",
    "llm:postprocess_started",
    "llm:postprocess_complete",
    "tts:synthesis_started",
    "tts:synthesis_complete",
    "output:text_emitted",
    "system:error",
    "system:session_started",
]


@dataclass(frozen=True, slots=True)
class Event:
    type: EventType
    payload: dict
    timestamp: datetime
    session_id: str
    sequence: int
    source: str

    @classmethod
    def create(
        cls,
        type: EventType,
        payload: dict,
        session_id: str = "",
        source: str = "unknown",
    ) -> "Event":
        return cls(
            type=type,
            payload=payload,
            timestamp=datetime.now(timezone.utc),
            session_id=session_id or str(uuid.uuid4()),
            sequence=-1,
            source=source,
        )
