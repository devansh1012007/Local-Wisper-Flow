# Benchmarks

`lwf-bench` drives a synthetic 5 s utterance (16 kHz tone+noise word bursts) through the
portable pipeline stages: head/tail silence trim, an STT stage, and the cleanup pipeline
(spoken commands + LLM cleanup + dictionary + snippets). The STT stage on Linux core
builds is a CPU-bound synthetic provider (`workload_units` scales its loop), so these
numbers measure the portable glue and the synthetic proxy, NOT whisper.cpp inference.

Run it:

```sh
./build/lwf-bench [runs] [utterance_seconds] [workload_units]
```

## Measured on this dev box

Hardware: Linux container, gcc 14.2.0 -O2 (Release), 2 vCPU, Intel Xeon. 2026-10-26.

runs=20, utterance=5 s, default workload_units=6:

| stage   | p50 (ms) | p95 (ms) |
|---------|----------|----------|
| trim    | 0.00     | 0.00     |
| stt     | 0.27     | 0.28     |
| cleanup | 0.07     | 0.11     |
| total   | 0.34     | 0.38     |

runs=20, utterance=5 s, workload_units=300 (heavier STT proxy):

| stage   | p50 (ms) | p95 (ms) |
|---------|----------|----------|
| trim    | 0.00     | 0.00     |
| stt     | 13.60    | 13.67    |
| cleanup | 0.10     | 0.20     |
| total   | 13.69    | 13.87    |

Interpretation: the portable pipeline overhead (trim, cleanup text path, engine glue) is
sub-millisecond; the latency budget is dominated by real STT/LLM inference, which must be
measured on Windows with the actual models (whisper ggml-base, qwen 1.5B). Real-hardware
numbers go here once Devansh runs `lwf-bench` and the app on a Windows machine with
`LWF_BUILD_FULL=ON`; the target remains capture-stop to visible text < 1.5 s CPU /
< 0.8 s GPU for a 5 s utterance.

## Cloud STT proxy (lwf-cloud-bench, S6)

`lwf-cloud-bench` measures the offline half of the cloud path with a loopback transport
(no real network): WAV encoding, multipart request build, clip splitting, the full
GnaniProvider round-trip with an injected per-request RTT, and the router/consent-gate
overhead with an instant fake cloud. The server-side inference time is NOT measured -
that needs the live API (checkpoint S0). Run:

```sh
./build/lwf-cloud-bench [runs] [utterance_seconds] [rtt_ms] [price_per_hour]
```

Measured on this dev box (same container as above), 2026-11-04:
runs=10, utterance=30 s, rtt_ms=100, price=0.18:

| stage                | p50 (ms) | p95 (ms) |
|----------------------|----------|----------|
| wav_encode           | 3.10     | 3.40     |
| request_build        | 4.65     | 4.77     |
| clip_split           | 0.00     | 0.00     |
| provider_roundtrip   | 204.31   | 204.41   |
| router_gate_overhead | 0.00     | 0.00     |

clips=2 (the synthetic utterance has a silence boundary near 15 s), ~469 KB sent per
clip. Reading: client-side work is ~8 ms per 30 s clip; the round-trip is dominated by
`clips x rtt` (2 x 100 ms here) plus real server time. A 5-minute dictation at 29.5 s
clips is ~11 sequential requests, so per-clip latency matters more than encoding.

Cost model (formula, not a quote): `cost = audio_hours x price_per_hour`. With the
verified Muse Voice Transcribe price of $0.18 per audio hour (Meta Model API pricing
page, 2026-11-04), a 30 s utterance costs $0.0015 and 1000 audio hours cost $180.
Gnani and ElevenLabs pricing is not modeled here - pass a price_per_hour value from
the vendor's current page when comparing providers.
