from __future__ import annotations

from typing import Callable, Optional

import numpy as np

from vision_sst.events import Event, EventType


class AudioService:
    """A minimal, deterministic audio service used for Phase 0 scaffolding."""

    def __init__(self, on_event: Optional[Callable[[Event], None]] = None):
        self.on_event = on_event
        self._running = False

    def start(self) -> None:
        self._running = True
        self._emit("audio:capture_started", {})

    def stop(self) -> None:
        self._running = False
        self._emit("audio:capture_stopped", {})

    def process_chunk(self, audio: np.ndarray) -> None:
        if not self._running:
            return
        level = float(np.sqrt(np.mean(audio.astype(np.float32) ** 2)))
        self._emit("audio:level", {"db": round(20 * np.log10(level + 1e-10), 3)})

    def _emit(self, event_type: EventType, payload: dict) -> None:
        if self.on_event:
            self.on_event(Event.create(event_type, payload, source="audio_service"))
