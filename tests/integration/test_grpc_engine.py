from vision_sst.engine.grpc_client import EngineClient
from vision_sst.engine.grpc_server import create_server


def test_grpc_engine_round_trip():
    server = create_server()
    port = server.add_insecure_port("[::]:0")
    server.start()

    try:
        client = EngineClient(target=f"localhost:{port}")
        session = client.start_session(mode="toggle", language="en")
        assert session.id

        status = client.get_status()
        assert status.state
        assert isinstance(status.model_loaded, bool)

        history = client.get_history(limit=5)
        assert isinstance(history, list)
        assert history

        events = client.stream_events(limit=1)
        assert events
    finally:
        client.close()
        server.stop(None)
