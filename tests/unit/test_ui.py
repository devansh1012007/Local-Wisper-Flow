from vision_sst.engine.service import EngineService
from vision_sst.ui import build_dashboard_html, build_status_payload
from vision_sst.ui_server import start_session, stop_session


def test_build_dashboard_html_contains_ui_sections():
    html = build_dashboard_html()

    assert "Vision SST Control Center" in html
    assert "Start Listening" in html
    assert "Live Event Stream" in html


def test_build_dashboard_html_contains_interactive_controls():
    html = build_dashboard_html()

    assert 'id="start-session"' in html
    assert 'id="stop-session"' in html
    assert "/api/status" in html


def test_build_status_payload_contains_runtime_details():
    service = EngineService()
    payload = build_status_payload(service)

    assert payload["state"] in {"idle", "armed", "listening", "processing", "error"}
    assert "audio_level" in payload
    assert "session_count" in payload


def test_ui_server_actions_manage_sessions():
    started = start_session()
    assert started["active"] is True

    stopped = stop_session()
    assert stopped["active"] is False
