from __future__ import annotations

import json
import os
from pathlib import Path
from tempfile import gettempdir
from typing import Any


class SessionStore:
    def __init__(self, path: str | None = None) -> None:
        default_path = Path(gettempdir()) / "vision_sst_session_store.json"
        configured = path or os.getenv("VISION_SST_SESSION_STORE_PATH")
        self.path = Path(configured or str(default_path))
        self._data: dict[str, Any] = {"sessions": []}
        self._load()

    def _load(self) -> None:
        if not self.path.exists():
            return
        try:
            raw = self.path.read_text(encoding="utf-8")
            data = json.loads(raw)
        except (json.JSONDecodeError, OSError):
            return
        if isinstance(data, dict):
            self._data = data

    def save(self) -> None:
        self.path.write_text(json.dumps(self._data, indent=2), encoding="utf-8")

    def add_session(self, session_id: str, mode: str, language: str) -> None:
        self._data.setdefault("sessions", []).append(
            {
                "id": session_id,
                "mode": mode,
                "language": language,
                "status": "active",
                "preview": "",
            }
        )
        self.save()

    def update_session(self, session_id: str, **updates: Any) -> None:
        for session in self._data.setdefault("sessions", []):
            if session.get("id") == session_id:
                session.update(updates)
                self.save()
                return

    def list_sessions(self) -> list[dict[str, Any]]:
        return list(self._data.get("sessions", []))
