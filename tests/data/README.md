# tests/data — local-only test assets

Everything in this directory is git-ignored except this README, so recordings
and fixtures can never leak into commits.

## Gnani smoke test assets

`scripts/gnani_smoke.py` accepts a recorded clip via `--file`. Requirements
imposed by the Gnani Prisma STT v3 contract:

- WAV, 16-bit PCM (not float), single channel (mono)
- 16 000 Hz sample rate
- at most 30 seconds (the endpoint rejects longer audio)

Produce a compliant clip from any recording with ffmpeg:

```sh
ffmpeg -i raw.wav -ar 16000 -ac 1 -c:a pcm_s16le tests/data/sample.wav
```

or with sox:

```sh
sox raw.wav -r 16000 -c 1 -b 16 tests/data/sample.wav
```

A 10–15 second clip of natural speech in the language you plan to dictate in
gives the most useful contract check. Silence also works: it exercises the
"empty transcript is a success, not an error" branch.

## Rules

- Never commit audio files, transcripts of real speech, or API keys.
- The smoke script reads the key only from the `GNANI_API_KEY` environment
  variable; never paste it on the command line or into files under this tree.
- Files written by `config_test.cpp` (config_test.json) are throwaway and are
  re-created on every run.
