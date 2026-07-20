from __future__ import annotations

from typing import Any

import grpc

from vision_sst.engine import engine_pb2, engine_pb2_grpc


class _ProtoCompat:
    def __getattr__(self, name: str) -> Any:
        return getattr(engine_pb2, name)


PROTO = _ProtoCompat()


class EngineClient:
    def __init__(self, target: str = "localhost:50051"):
        self._channel = grpc.insecure_channel(target)
        self._stub = engine_pb2_grpc.EngineStub(self._channel)

    def start_session(self, mode: str = "toggle", language: str = "") -> Any:
        request = PROTO.StartRequest(mode=mode, language=language)
        return self._stub.StartSession(request)

    def stop_session(self, session_id: str) -> Any:
        request = PROTO.StopRequest(session_id=session_id)
        return self._stub.StopSession(request)

    def get_status(self) -> Any:
        return self._stub.GetStatus(PROTO.Empty())

    def stream_events(self, limit: int = 1) -> list[Any]:
        events = []
        for event in self._stub.StreamEvents(PROTO.Empty()):
            events.append(event)
            if len(events) >= limit:
                break
        return events

    def get_history(self, limit: int = 10) -> list[dict[str, Any]]:
        response = self._stub.GetHistory(PROTO.HistoryRequest(limit=limit))
        return [
            {"id": item.id, "preview": item.preview, "duration_ms": item.duration_ms}
            for item in response.items
        ]

    def close(self) -> None:
        self._channel.close()
