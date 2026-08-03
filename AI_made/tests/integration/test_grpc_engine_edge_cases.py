from vision_sst.engine.grpc_client import EngineClient
from vision_sst.engine.grpc_server import create_server


def test_grpc_engine_handles_empty_history_query_and_stop_session():
    server = create_server()
    port = server.add_insecure_port("[::]:0")
    server.start()

    try:
        client = EngineClient(target=f"localhost:{port}")
        session = client.start_session(mode="toggle", language="en")
        client.stop_session(session.id)

        history = client.get_history(limit=5)
        assert isinstance(history, list)
        assert history

        status = client.get_status()
        assert status.state in {"idle", "armed", "capturing", "transcribing", "postprocessing", "emitting"}
    finally:
        client.close()
        server.stop(None)
