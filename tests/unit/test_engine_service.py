from vision_sst.engine.service import EngineService
from vision_sst.events import Event
from vision_sst.state_machine import State


def test_engine_service_start_stop_and_history():
    service = EngineService()

    session = service.start_session(mode="toggle", language="en")
    assert session.mode == "toggle"
    assert session.language == "en"

    service.stop_session(session.id)
    history = service.get_history(limit=5)
    assert any(item["id"] == session.id for item in history)


def test_engine_service_tracks_audio_level_and_fsm_transition():
    service = EngineService()

    service.add_event(Event.create("audio:level", {"db": -12.5}, session_id="s1", source="test"))
    status = service.get_status()

    assert status["audio_level"] == -12.5

    assert service.process_transition("HOTKEY_PRESS") is True
    assert service.fsm.state is State.ARMED
