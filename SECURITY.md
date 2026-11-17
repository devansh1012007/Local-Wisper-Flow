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

## Update channel trust

The in-app update check (Settings > General > Check for updates) reads
`latest.json` from this repository's newest GitHub Release over HTTPS. That
manifest is **not cryptographically signed**: a compromised GitHub account or
a TLS-level attacker could serve a modified file. This is an accepted,
documented limit because the check is manual and inert by design:

- it only compares version strings (`src/core/VersionCheck.cpp`);
- it never downloads, writes or executes anything itself;
- asset URLs from the manifest are only accepted when `https:`, otherwise it
  falls back to this repository's hard-coded releases page;
- "Open download page" opens the browser; the actual download still goes
  through GitHub and should be verified with `SHA256SUMS.txt` as above.

Before any future release adds an automatic download or install of updates,
the manifest must gain a cryptographic signature (for example minisign or
cosign over `latest.json`, public key baked into the app) and the app must
verify that signature before acting on the manifest. Until then the update
check stays a passive, user-initiated action.

## Supply chain

GitHub Actions in `.github/workflows/` are pinned to commit SHAs (with the
version kept as a comment), not moving tags - the release job holds the
repository write token and the signing secrets, so a moved `actions/*` tag
must never silently change what runs there. Model download URLs in
`models.json` are pinned to upstream commit hashes for the same reason; the
pinned sha256 values are verified by `scripts/verify_model_hashes.sh` before
each release.

## Supported versions

Only the latest tagged release receives security fixes.
