# How to apply the cloud STT patches

The patch set lives in this directory:

- `0001-…patch` … `0007-…patch` — the full commit series (`git format-patch`),
  one commit per task-chain step (Phase 0 docs, S0, S1, S2, S3, S4).
- `stt-providers-combined.patch` — the same total change as one diff
  (`git diff e60cd2a..feature/stt-providers`).
- `PR_DESCRIPTION.md` — ready-to-paste pull-request body.

Everything applies cleanly on top of `main` at `e60cd2a` (verified with a fresh
worktree + `git am`; resulting tree hash is identical to the source branch).

## Option A — keep the commit history (recommended)

From your Local-Wisper-Flow checkout on `main` at `e60cd2a`:

```sh
git checkout -b feature/stt-providers
git am /path/to/stt-providers/0*.patch
```

The commits were authored by the agent as `zai-agent <agent@z.ai>`. To take
authorship yourself:

```sh
git rebase --exec 'git commit --amend --reset-author --no-edit' e60cd2a
```

(with `user.name`/`user.email` set to your identity; rebase non-interactively
replays the same patches).

## Option B — one squashed commit

```sh
git checkout -b feature/stt-providers
git apply /path/to/stt-providers/stt-providers-combined.patch
git add -A
git commit -m "Add opt-in cloud STT (Gnani Prisma) with consent gating and local fallback"
```

## Option C — inspect before applying

```sh
git apply --stat /path/to/stt-providers/stt-providers-combined.patch
git apply --check /path/to/stt-providers/stt-providers-combined.patch
```

## After applying, verify locally

```sh
sh scripts/check_hygiene.sh

# Linux (portable suite, 140 cases)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DLLAMA_URL=file://$PWD/_deps-cache/llama-b10603.tar.gz
cmake --build build -j4 --target lwf-tests
ctest --test-dir build --output-on-failure

# Windows (full app incl. WinHTTP client)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Then verify the live Gnani contract (S0 checkpoint)

```sh
ffmpeg -i your-recording.wav -ar 16000 -ac 1 -c:a pcm_s16le tests/data/sample.wav
GNANI_API_KEY=... python3 scripts/gnani_smoke.py --file tests/data/sample.wav
```

If the live response shape differs from the assumed one, the single places to
adjust are `src/stt/cloud/GnaniContract.h` (endpoint, header names, form field
names) and the tolerant transcript lookup in `src/stt/cloud/GnaniResponse.cpp`.
`python3 scripts/gnani_smoke.py --dry-run` prints the exact request layout
without spending a request.

## Trying it end to end (Windows)

1. `setx GNANI_API_KEY <your-key>` (or set it in your shell).
2. In `config.json` (the one next to the exe or under `%APPDATA%\LocalWisperFlow`):
   `"stt_provider": "gnani", "stt_consent_accepted": true`.
3. Run the app; the console shows `[stt] cloud provider gnani enabled`.
   Failures log one privacy-scrubbed line each and fall back to local.
