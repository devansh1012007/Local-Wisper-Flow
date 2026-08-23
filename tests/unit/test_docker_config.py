from pathlib import Path

from vision_sst.__main__ import resolve_port
from vision_sst.engine.history import HistoryStore
from vision_sst.engine.store import SessionStore


def test_dockerfile_and_ignore_exist():
    root = Path(__file__).resolve().parents[2]
    dockerfile = root / "Dockerfile"
    ignore = root / ".dockerignore"

    assert dockerfile.exists()
    assert ignore.exists()


def test_runtime_configuration_uses_environment(monkeypatch, tmp_path):
    monkeypatch.setenv("VISION_SST_PORT", "60061")
    monkeypatch.setenv("VISION_SST_SESSION_STORE_PATH", str(tmp_path / "sessions.json"))
    monkeypatch.setenv("VISION_SST_HISTORY_STORE_PATH", str(tmp_path / "history.json"))

    assert resolve_port() == 60061

    session_store = SessionStore()
    history_store = HistoryStore()

    assert session_store.path == tmp_path / "sessions.json"
    assert history_store.path == tmp_path / "history.json"
