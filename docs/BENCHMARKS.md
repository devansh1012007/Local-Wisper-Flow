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
