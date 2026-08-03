from __future__ import annotations

import argparse
import os

from vision_sst.engine.grpc_server import create_server


def resolve_port(default: int = 50051) -> int:
    value = os.getenv("VISION_SST_PORT")
    if value is None:
        return default
    try:
        return int(value)
    except ValueError:
        return default


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the Vision SST engine")
    parser.add_argument("--port", type=int, default=resolve_port(), help="Port for the gRPC server")
    args = parser.parse_args()

    server = create_server()
    port = server.add_insecure_port(f"[::]:{args.port}")
    server.start()
    print(f"Engine listening on port {port}")
    server.wait_for_termination()


if __name__ == "__main__":
    main()
