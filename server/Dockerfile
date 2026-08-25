# Lightweight gRPC inference server for Local Wisper Flow.
#
# Layer-caching: requirements are installed BEFORE the source copy so
# code edits do not invalidate the (heavy) pip layer.

FROM python:3.11-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# Build essentials + ffmpeg cover faster-whisper's runtime needs
# (ffmpeg is only required for non-PCM inputs, but keeps the image future-proof).
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY server/requirements.txt /app/server/requirements.txt
RUN pip install --no-cache-dir -r server/requirements.txt

COPY server/inference.proto /app/server/inference.proto
COPY server/server.py /app/server/server.py

# Generate the Python stubs at image build time so the container needs
# no grpcio-tools at runtime.
RUN cd server \
    && python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. inference.proto

WORKDIR /app/server

EXPOSE 50051

CMD ["python", "server.py"]
