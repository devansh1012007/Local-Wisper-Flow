#!/usr/bin/env python3
"""Gnani Prisma STT contract smoke test.

Sends one multipart POST to the Gnani STT v3 endpoint exactly the way the C++
client (src/stt/cloud/*) will, then prints the observed contract: HTTP status,
response headers (to locate the request id header), parsed body shape, byte
counts and latency. Run it with a real key before wiring the C++ provider:

    GNANI_API_KEY=... python3 scripts/gnani_smoke.py --file tests/data/sample.wav

With no --file it synthesizes silence in memory, which exercises the
"empty transcript is a success" contract branch. Nothing is written to disk.
The API key is read only from the environment and never printed.

Keep the constants below in sync with src/stt/cloud/GnaniContract.h.
"""

import argparse
import json
import math
import os
import struct
import sys
import time
import urllib.error
import urllib.request
import wave

ENDPOINT = "https://api.vachana.ai/stt/v3"
API_KEY_ENV = "GNANI_API_KEY"
API_KEY_HEADER = "X-API-Key-ID"
MODEL = "prisma-v3.0"
DEFAULT_LANGUAGE = "indic-auto"
SAMPLE_RATE = 16000
MAX_AUDIO_SECONDS = 30
BOUNDARY = "lwf-smoke-boundary-7f3a9c1e"
AUDIO_PART_NAME = "audio"
AUDIO_FILENAME = "audio.wav"
KEYTERMS_FIELD = "keyterms"
KEYTERMS_CAP = 200
REQUEST_ID_HEADERS = ("x-request-id", "request-id")
BODY_PRINT_CAP = 2000

EXIT_OK = 0
EXIT_HTTP_ERROR = 1
EXIT_NETWORK = 2
EXIT_NO_KEY = 3
EXIT_BAD_INPUT = 4
EXIT_CONTRACT_ANOMALY = 5


def wav_bytes(pcm16_le):
    data_size = len(pcm16_le)
    header = b"RIFF" + struct.pack("<I", 36 + data_size) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE,
                                    SAMPLE_RATE * 2, 2, 16)
    header += b"data" + struct.pack("<I", data_size)
    return header + pcm16_le


def synth_silence(seconds):
    return wav_bytes(b"\x00\x00" * (seconds * SAMPLE_RATE))


def synth_tone(seconds):
    pcm = bytearray()
    for i in range(seconds * SAMPLE_RATE):
        sample = int(6000.0 * math.sin(2.0 * math.pi * 440.0 * i / SAMPLE_RATE))
        pcm += struct.pack("<h", sample)
    return wav_bytes(bytes(pcm))


def load_wav(path):
    with wave.open(path, "rb") as reader:
        if reader.getnchannels() != 1:
            raise ValueError("expected mono audio, got %d channels" % reader.getnchannels())
        if reader.getsampwidth() != 2:
            raise ValueError("expected 16-bit PCM, got %d bytes per sample" % reader.getsampwidth())
        if reader.getframerate() != SAMPLE_RATE:
            raise ValueError("expected %d Hz, got %d Hz" % (SAMPLE_RATE, reader.getframerate()))
        frames = reader.getnframes()
        if frames > MAX_AUDIO_SECONDS * SAMPLE_RATE:
            raise ValueError("clip is %d s, Gnani accepts at most %d s"
                             % (frames // SAMPLE_RATE, MAX_AUDIO_SECONDS))
        return wav_bytes(reader.readframes(frames))


def build_multipart(fields, audio, boundary):
    body = bytearray()
    for name, value in fields.items():
        body += ("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"
                 % (boundary, name, value)).encode("utf-8")
    body += ("--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
             "Content-Type: audio/wav\r\n\r\n"
             % (boundary, AUDIO_PART_NAME, AUDIO_FILENAME)).encode("utf-8")
    body += audio
    body += ("\r\n--%s--\r\n" % boundary).encode("utf-8")
    return bytes(body)


def find_transcript(obj):
    for key in ("transcript", "text"):
        if isinstance(obj.get(key), str):
            return obj[key]
    data = obj.get("data")
    if isinstance(data, dict):
        for key in ("transcript", "text"):
            if isinstance(data.get(key), str):
                return data[key]
    return None


def find_error(obj):
    err = obj.get("error")
    if isinstance(err, dict):
        return {"code": err.get("code") if err.get("code") is not None else err.get("type"),
                "message": err.get("message")}
    if isinstance(err, str):
        return {"code": None, "message": err}
    for key in ("message", "detail"):
        if isinstance(obj.get(key), str):
            return {"code": None, "message": obj[key]}
    return None


def find_request_id(headers):
    for name in REQUEST_ID_HEADERS:
        for key, value in headers.items():
            if key.lower() == name:
                return value
    return None


def safe_headers(headers, secret):
    out = {}
    for key, value in headers.items():
        out[key] = "***" if secret and secret in str(value) else value
    return out


def print_request_plan(args, key, audio, body):
    print("endpoint:        %s" % args.endpoint)
    print("api key:         %s" % ("set (from %s, never printed)" % API_KEY_ENV if key
                                   else "MISSING (%s unset)" % API_KEY_ENV))
    print("auth header:     %s" % API_KEY_HEADER)
    print("content-type:    multipart/form-data; boundary=%s" % BOUNDARY)
    print("model:           %s" % args.model)
    print("language:        %s" % args.language)
    if args.keyterms:
        terms = [term.strip() for term in args.keyterms.split(",") if term.strip()]
        print("keyterms:        %d term(s), capped at %d" % (len(terms), KEYTERMS_CAP))
    print("audio:           %d bytes wav, %d ms, 16-bit PCM mono %d Hz"
          % (len(audio), len(audio) * 1000 // (SAMPLE_RATE * 2), SAMPLE_RATE))
    print("body:            %d bytes" % len(body))


def main():
    parser = argparse.ArgumentParser(description="Gnani Prisma STT contract smoke test")
    parser.add_argument("--file", help="path to a 16 kHz mono 16-bit PCM wav (<= 30 s)")
    parser.add_argument("--seconds", type=int, default=2,
                        help="seconds of synthesized audio when no --file is given")
    parser.add_argument("--tone", action="store_true",
                        help="synthesize a 440 Hz tone instead of silence")
    parser.add_argument("--language", default=DEFAULT_LANGUAGE,
                        help="language code, e.g. %s or a comma list" % DEFAULT_LANGUAGE)
    parser.add_argument("--model", default=MODEL)
    parser.add_argument("--endpoint", default=ENDPOINT)
    parser.add_argument("--keyterms", default="",
                        help="comma-separated hint words, capped at %d" % KEYTERMS_CAP)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--dry-run", action="store_true",
                        help="print the request layout without touching the network")
    args = parser.parse_args()

    try:
        if args.file:
            audio = load_wav(args.file)
        elif args.tone:
            audio = synth_tone(max(1, min(args.seconds, MAX_AUDIO_SECONDS)))
        else:
            audio = synth_silence(max(1, min(args.seconds, MAX_AUDIO_SECONDS)))
    except (OSError, ValueError, wave.Error) as exc:
        print("input error: %s" % exc)
        return EXIT_BAD_INPUT

    fields = {"model": args.model, "language": args.language}
    if args.keyterms:
        terms = [term.strip() for term in args.keyterms.split(",") if term.strip()]
        fields[KEYTERMS_FIELD] = ",".join(terms[:KEYTERMS_CAP])

    body = build_multipart(fields, audio, BOUNDARY)
    key = os.environ.get(API_KEY_ENV)

    if args.dry_run:
        print_request_plan(args, key, audio, body)
        print("dry run: no request was sent")
        return EXIT_OK

    if not key:
        print("missing API key: set %s in the environment" % API_KEY_ENV)
        return EXIT_NO_KEY

    request = urllib.request.Request(args.endpoint, data=body, method="POST")
    request.add_header(API_KEY_HEADER, key)
    request.add_header("Content-Type", "multipart/form-data; boundary=%s" % BOUNDARY)

    started = time.perf_counter()
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            status = response.status
            headers = dict(response.headers.items())
            payload = response.read()
    except urllib.error.HTTPError as exc:
        status = exc.code
        headers = dict(exc.headers.items())
        payload = exc.read()
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        print("network error: %s" % exc)
        return EXIT_NETWORK
    latency_ms = int((time.perf_counter() - started) * 1000)

    print("http status:     %d" % status)
    print("latency:         %d ms" % latency_ms)
    print("bytes sent:      %d" % len(body))
    print("bytes received:  %d" % len(payload))
    request_id = find_request_id(headers)
    print("request id:      %s" % (request_id if request_id else "(none found in %s)"
                                   % "/".join(REQUEST_ID_HEADERS)))
    print("response headers:")
    for name, value in sorted(safe_headers(headers, key).items()):
        print("  %s: %s" % (name, value))

    text = payload.decode("utf-8", errors="replace")
    try:
        parsed = json.loads(text)
    except ValueError:
        print("body is not JSON (first %d chars): %s" % (BODY_PRINT_CAP, text[:BODY_PRINT_CAP]))
        return EXIT_CONTRACT_ANOMALY if status < 400 else EXIT_HTTP_ERROR

    print("body shape:      %s" % ", ".join(sorted(parsed.keys())) if isinstance(parsed, dict)
          else "non-object JSON")
    if status >= 400:
        error = find_error(parsed) if isinstance(parsed, dict) else None
        print("parsed error:    %s" % json.dumps(error))
        print("raw body (first %d chars): %s" % (BODY_PRINT_CAP, text[:BODY_PRINT_CAP]))
        return EXIT_HTTP_ERROR

    transcript = find_transcript(parsed) if isinstance(parsed, dict) else None
    if transcript is None:
        print("no transcript/text field found in a 2xx body; report this shape:")
        print(text[:BODY_PRINT_CAP])
        return EXIT_CONTRACT_ANOMALY

    if transcript == "":
        print("transcript:      (empty - success per contract, not an error)")
    else:
        print("transcript:      %s" % transcript)
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
