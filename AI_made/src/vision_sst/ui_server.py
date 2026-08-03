from __future__ import annotations

import http.server
import json
import socketserver
from typing import Any

from vision_sst.engine.service import EngineService
from vision_sst.ui import build_dashboard_html, build_status_payload

_engine_service = EngineService()


class UIRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self) -> None:  # noqa: N802
        if self.path in {"/", "/index.html"}:
            body = build_dashboard_html().encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/api/status":
            payload = build_status_payload(_engine_service)
            body = json.dumps(payload).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_error(404)

    def do_POST(self) -> None:  # noqa: N802
        if self.path == "/api/start":
            self._send_json(start_session())
            return

        if self.path == "/api/stop":
            self._send_json(stop_session())
            return

        self.send_error(404)

    def _send_json(self, payload: dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format: str, *args: Any) -> None:
        return


def start_session() -> dict[str, Any]:
    session = _engine_service.start_session(mode="toggle", language="en")
    return {"active": True, "session_id": session.id}


def stop_session() -> dict[str, Any]:
    sessions = [session for session in _engine_service._sessions if session.status == "active"]
    if sessions:
        target = sessions[-1]
        _engine_service.stop_session(target.id)
        return {"active": False, "session_id": target.id}
    return {"active": False, "session_id": None}


def serve(host: str = "0.0.0.0", port: int = 8000) -> None:
    with socketserver.TCPServer((host, port), UIRequestHandler) as httpd:
        print(f"UI listening on http://{host}:{port}")
        httpd.serve_forever()


if __name__ == "__main__":
    serve()
