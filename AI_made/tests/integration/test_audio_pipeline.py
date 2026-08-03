import numpy as np

from vision_sst.events import Event
from vision_sst.services.audio import AudioService


def test_audio_service_emits_level_events_when_running():
    events: list[Event] = []
    service = AudioService(on_event=events.append)

    service.start()
    service.process_chunk(np.array([0.1, -0.2, 0.3, -0.4], dtype=np.float32))
    service.stop()

    assert len(events) >= 2
    assert any(event.type == "audio:level" for event in events)
