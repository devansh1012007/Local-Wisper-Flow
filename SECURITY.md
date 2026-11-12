# Security policy

## Reporting a vulnerability

Open a GitHub Security Advisory ("Report a vulnerability" on the repository's
Security tab) or open a regular issue describing the impact without
exploitation details. Please do not post working exploits publicly. You will
get a response within a few days; fixes ship in the next tagged release.

## Threat model (what this app is, and is not)

Echo flow is a local-first dictation app:

- Audio is captured, transcribed (whisper) and - optionally - cleaned up
  (local LLM) entirely on this machine. Nothing is sent anywhere unless you
  explicitly enable a cloud provider (Settings > Cloud) or the optional
  OpenRouter cleanup path, both of which require a consent flag AND an API
  key you provide.
- No telemetry, no crash uploads, no analytics. Crash reports are written to
  the local logs directory and never leave the machine.
- API keys live in `api_keys.json` next to `config.json` under
  `%APPDATA%\EchoFlow` (or in the environment), never in `config.json`,
  redacted in the GUI and in every log line.
- A privacy gate in CI (`scripts/check_no_network.sh`) fails the build when
  network APIs appear outside the three allowlisted seams (model downloader,
  cloud STT HTTP client, gRPC inference client) or when cloud endpoint names
  appear outside `src/stt/cloud/`.

## Verify a release download

Every tagged release publishes a `SHA256SUMS.txt` covering the installer and
the portable zip. Verify before running:

```sh
sha256sum -c SHA256SUMS.txt          # Linux / MSYS2
Get-FileHash -Algorithm SHA256 <file>  # PowerShell
```

When releases are code-signed (see `docs/RELEASE.md`), also verify the
Authenticode signature:

```powershell
Get-AuthenticodeSignature .\EchoFlowSetup-<version>.exe
```

Only run installers downloaded from this repository's GitHub Releases page.

## Supported versions

Only the latest tagged release receives security fixes.
