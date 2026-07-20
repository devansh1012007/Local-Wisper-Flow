from pathlib import Path

from vision_sst.engine.store import SessionStore


def test_session_store_persists_sessions(tmp_path: Path):
    path = tmp_path / "store.json"
    store = SessionStore(path=str(path))

    store.add_session("s1", mode="toggle", language="en")
    store.update_session("s1", status="stopped", preview="hello")

    reloaded = SessionStore(path=str(path))
    sessions = reloaded.list_sessions()

    assert len(sessions) == 1
    assert sessions[0]["status"] == "stopped"
    assert sessions[0]["preview"] == "hello"
