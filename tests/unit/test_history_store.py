from pathlib import Path

from vision_sst.engine.history import HistoryStore, SessionSnapshot


def test_history_store_round_trip(tmp_path: Path):
    path = tmp_path / "history.json"
    store = HistoryStore(path=str(path))

    store.add(SessionSnapshot(id="s1", mode="toggle", language="en"))
    store.update("s1", status="stopped", preview="hello")

    reloaded = HistoryStore(path=str(path))
    snapshots = reloaded.list()

    assert len(snapshots) == 1
    assert snapshots[0].status == "stopped"
    assert snapshots[0].preview == "hello"
