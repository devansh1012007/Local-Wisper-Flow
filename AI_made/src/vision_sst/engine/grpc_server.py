from __future__ import annotations

import grpc
from concurrent import futures

from vision_sst.engine.service import EngineService
from vision_sst.engine import engine_pb2, engine_pb2_grpc


class EngineServiceServicer(engine_pb2_grpc.EngineServicer):
    def __init__(self, service: EngineService | None = None):
        self._service = service or EngineService()

    def StartSession(self, request, context):
        session = self._service.start_session(mode=request.mode, language=request.language)
        return engine_pb2.Session(id=session.id, status=session.status, started_at=int(session.started_at.timestamp()))

    def StopSession(self, request, context):
        self._service.stop_session(request.session_id)
        return engine_pb2.Empty()

    def GetStatus(self, request, context):
        status = self._service.get_status()
        return engine_pb2.Status(state=status["state"], model_loaded=status["model_loaded"], audio_level=float(status["audio_level"]))

    def StreamEvents(self, request, context):
        for event in self._service.get_events(limit=5):
            yield engine_pb2.Event(
                type=event.type,
                payload_json="{}",
                timestamp=int(event.timestamp.timestamp()),
                session_id=event.session_id,
            )

    def GetHistory(self, request, context):
        items = self._service.get_history(query=request.query, limit=request.limit or 10)
        return engine_pb2.HistoryResponse(items=[engine_pb2.SessionItem(id=item["id"], preview=item["preview"], duration_ms=item["duration_ms"]) for item in items])


def create_server() -> grpc.Server:
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    engine_pb2_grpc.add_EngineServicer_to_server(EngineServiceServicer(), server)
    return server
