from datetime import datetime

from vision_sst.events import Event


def test_event_create_populates_defaults():
    event = Event.create("audio:level", {"db": -12.0}, session_id="session-1", source="audio")

    assert event.type == "audio:level"
    assert event.payload == {"db": -12.0}
    assert event.session_id == "session-1"
    assert event.sequence == -1
    assert isinstance(event.timestamp, datetime)
    assert event.source == "audio"
