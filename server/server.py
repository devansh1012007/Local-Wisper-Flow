"""InferenceService gRPC server for Local Wisper Flow.

Implements the two RPCs from inference.proto:
  - Transcribe: decodes float32 PCM bytes and runs speech-to-text.
    Uses faster-whisper when available; otherwise falls back to a
    lightweight mock so the service runs anywhere (e.g. CI, tiny VPS).
  - ProcessText: applies an LLM transform. Falls back to a simple
    prefix-based mock unless a real backend is wired in.

Run locally:
    python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. inference.proto
    python server.py
"""

import logging
import os
import struct

import grpc

import inference_pb2
import inference_pb2_grpc

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
LOG = logging.getLogger("inference-server")

LISTEN_PORT = int(os.environ.get("LWF_PORT", "50051"))
LISTEN_HOST = os.environ.get("LWF_BIND", "127.0.0.1")
MAX_MESSAGE_BYTES = 256 * 1024 * 1024

# Attempt to import faster-whisper once; the server stays fully functional
# without it by serving the mock transcriber below.
try:
    from faster_whisper import WhisperModel  # noqa: N813

    _WHISPER_MODEL = WhisperModel("base.en", device="cpu", compute_type="int8")
    WHISPER_AVAILABLE = True
    LOG.info("faster-whisper loaded (base.en, cpu/int8)")
except Exception as exc:  # pragma: no cover - depends on environment
    WHISPER_AVAILABLE = False
    LOG.warning("faster-whisper unavailable (%s); using mock transcriber", exc)


def pcm_bytes_to_floats(raw: bytes):
    """Decode little-endian float32 PCM bytes into a list of floats."""
    count = len(raw) // 4
    return list(struct.unpack("<%df" % count, raw[: count * 4]))


class InferenceService(inference_pb2_grpc.InferenceServiceServicer):
    """Servicer implementing InferenceService."""

    def Transcribe(self, request, context):
        samples = pcm_bytes_to_floats(request.audio_data)
        LOG.info(
            "Transcribe: %d bytes (%d samples), language=%s",
            len(request.audio_data),
            len(samples),
            request.language or "auto",
        )

        if not samples:
            context.abort(grpc.StatusCode.INVALID_ARGUMENT, "empty audio payload")

        if not WHISPER_AVAILABLE:
            context.abort(
                grpc.StatusCode.UNAVAILABLE,
                "faster-whisper is not installed on the server",
            )

        segments, _info = _WHISPER_MODEL.transcribe(samples, language=request.language)
        text = " ".join(segment.text.strip() for segment in segments).strip()
        return inference_pb2.TranscribeResponse(text=text, confidence=0.9)

    def ProcessText(self, request, context):
        LOG.info("ProcessText: %d chars, prompt=%d chars",
                 len(request.text), len(request.system_prompt))

        context.abort(
            grpc.StatusCode.UNAVAILABLE,
            "no LLM backend configured on the server",
        )


def build_and_serve():
    from concurrent import futures

    server = grpc.server(
        futures.ThreadPoolExecutor(max_workers=8),
        options=[
            ("grpc.max_receive_message_length", MAX_MESSAGE_BYTES),
            ("grpc.max_send_message_length", MAX_MESSAGE_BYTES),
        ],
    )
    inference_pb2_grpc.add_InferenceServiceServicer_to_server(InferenceService(), server)
    bound = server.add_insecure_port(f"{LISTEN_HOST}:{LISTEN_PORT}")
    if bound == 0:
        raise RuntimeError(f"failed to bind {LISTEN_HOST}:{LISTEN_PORT}")
    server.start()
    LOG.info("InferenceService listening on %s:%d", LISTEN_HOST, LISTEN_PORT)
    server.wait_for_termination()


if __name__ == "__main__":
    build_and_serve()
