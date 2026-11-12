# Privacy

Echo flow collects nothing. There is no telemetry, no analytics, no crash
upload, no account.

## What stays on this machine

- Your audio: captured and transcribed locally by whisper. Never written to
  disk by the audio path (enforced by a CI gate, `check_no_network.sh`).
- Your text: history, dictionary, snippets, stats - all files live under
  `%APPDATA%\EchoFlow` on your machine. Deleting them removes every trace
  (the uninstaller asks).
- Crash reports: written to `%APPDATA%\EchoFlow\logs\` as plain text. They
  contain version info and a log tail, never transcripts or keys, and never
  leave your machine unless you attach them to a bug report yourself.

## What can leave this machine (only if you opt in)

| Feature | What is sent | Where | How to enable |
|---|---|---|---|
| Cloud speech-to-text (Gnani / ElevenLabs / Muse) | the recorded audio clip, language hint, your API key | the provider you chose | Settings > Cloud: accept the consent notice AND store a key |
| LLM cleanup via OpenRouter | the dictated text (not audio), your API key | openrouter.ai | Settings > Cleanup: choose OpenRouter, accept consent, store a key |

Both paths are OFF by default, require two explicit actions (consent + key),
and fall back to local processing on failure. When they fail, the error text
is redacted before it is stored or shown.

## Network traffic summary

With every cloud option off, the app performs no network requests at all -
except one: an explicit "Check for updates" (off by default as a daily
automatic check; see Settings > General). That check fetches a small version
file from GitHub; it sends no identifying information and downloads nothing
by itself. The model download (first run or Settings > Models) fetches the
weights you choose, verifies them against pinned SHA-256 hashes, and is also
an explicit action.

## Model downloads

The speech and language models listed in `models.json` are pinned by SHA-256.
A download whose hash does not match is refused and deleted. Model files come
from the official sources listed in `NOTICE.md`.
