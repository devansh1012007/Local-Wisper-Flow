from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
from tempfile import gettempdir
from typing import Any


@dataclass
class SessionSnapshot:
    id: str
    mode: str
    language: str
    status: str = "active"
    preview: str = ""


class HistoryStore:
    def __init__(self, path: str | None = None) -> None:
        default_path = Path(gettempdir()) / "vision_sst_history_store.json"
        configured = path or os.getenv("VISION_SST_HISTORY_STORE_PATH")
        self.path = Path(configured or str(default_path))
        self._records: list[SessionSnapshot] = []
        self._load()

    def _load(self) -> None:
        if not self.path.exists():
            return
        try:
            payload = json.loads(self.path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return
        if isinstance(payload, list):
            self._records = [SessionSnapshot(**item) for item in payload if isinstance(item, dict)]

    def save(self) -> None:
        payload = [item.__dict__ for item in self._records]
        self.path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def add(self, snapshot: SessionSnapshot) -> None:
        self._records.append(snapshot)
        self.save()

    def update(self, session_id: str, **updates: Any) -> None:
        for record in self._records:
            if record.id == session_id:
                for key, value in updates.items():
                    setattr(record, key, value)
                self.save()
                return

    def list(self) -> list[SessionSnapshot]:
        return list(self._records)
