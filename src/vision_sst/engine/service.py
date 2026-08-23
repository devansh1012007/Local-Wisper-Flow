from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from threading import Lock
import uuid

from vision_sst.engine.history import HistoryStore, SessionSnapshot
from vision_sst.engine.store import SessionStore
from vision_sst.events import Event
from vision_sst.state_machine import DictationFSM


@dataclass
class SessionRecord:
    id: str
    mode: str
    language: str
    started_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    status: str = "active"
    preview: str = ""


class EngineService:
    def __init__(self, store: SessionStore | None = None, history_store: HistoryStore | None = None) -> None:
        self.fsm = DictationFSM()
        self._lock = Lock()
        self._sessions: list[SessionRecord] = []
        self._events: list[Event] = []
        self._audio_level = 0.0
        self._model_loaded = False
        self._store = store or SessionStore()
        self._history_store = history_store or HistoryStore()
        self._load_from_store()

    def _load_from_store(self) -> None:
        for item in self._store.list_sessions():
            self._sessions.append(
                SessionRecord(
                    id=item["id"],
                    mode=item.get("mode", "toggle"),
                    language=item.get("language", ""),
                    status=item.get("status", "active"),
                    preview=item.get("preview", ""),
                )
            )

    def start_session(self, mode: str = "toggle", language: str = "") -> SessionRecord:
        with self._lock:
            session = SessionRecord(id=str(uuid.uuid4()), mode=mode, language=language)
            self._sessions.append(session)
            self._store.add_session(session.id, mode=mode, language=language)
            self._history_store.add(SessionSnapshot(id=session.id, mode=mode, language=language))
            self._events.append(Event.create("system:session_started", {"mode": mode, "language": language}, session_id=session.id, source="engine"))
            return session

    def stop_session(self, session_id: str) -> None:
        with self._lock:
            for session in self._sessions:
                if session.id == session_id:
                    session.status = "stopped"
                    self._store.update_session(session_id, status="stopped")
                    self._history_store.update(session_id, status="stopped")
                    break

    def get_status(self) -> dict:
        with self._lock:
            return {
                "state": self.fsm.state.name.lower(),
                "model_loaded": self._model_loaded,
                "audio_level": self._audio_level,
            }

    def add_event(self, event: Event) -> None:
        with self._lock:
            self._events.append(event)
            if event.type == "audio:level":
                self._audio_level = float(event.payload.get("db", 0.0))

    def get_events(self, limit: int = 5) -> list[Event]:
        with self._lock:
            return list(self._events[-limit:])

    def get_history(self, query: str = "", limit: int = 10) -> list[dict]:
        with self._lock:
            indexed_sessions = list(enumerate(self._sessions))
            ordered_sessions = sorted(
                indexed_sessions,
                key=lambda item: (item[1].started_at, item[0]),
                reverse=True,
            )
            items = []
            for _, session in ordered_sessions:
                preview = session.preview or ""
                if query and query.lower() not in preview.lower():
                    continue
                items.append({
                    "id": session.id,
                    "preview": preview,
                    "duration_ms": 0,
                })
            if not items:
                return [
                    {"id": item["id"], "preview": item.get("preview", ""), "duration_ms": 0}
                    for item in self._store.list_sessions()[:limit]
                ]
            return items[:limit]

    def process_transition(self, trigger_name: str) -> bool:
        with self._lock:
            trigger = None
            for candidate in [
                "HOTKEY_PRESS",
                "HOTKEY_RELEASE",
                "VAD_SPEECH",
                "VAD_SILENCE",
                "SST_COMPLETE",
                "SST_FAILED",
                "LLM_COMPLETE",
                "TIMEOUT",
                "ABORT",
            ]:
                if candidate == trigger_name.upper():
                    trigger = candidate
                    break
            if trigger is None:
                return False
            from vision_sst.state_machine import Trigger
            enum_trigger = getattr(Trigger, trigger)
            return self.fsm.trigger(enum_trigger)
