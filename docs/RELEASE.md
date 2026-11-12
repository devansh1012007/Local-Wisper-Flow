# Release process

From tag to published release. The release workflow
(`.github/workflows/release.yml`) automates steps 4-9 on every `v*` tag;
this document is the checklist and the manual fallback.

## 1. Version bump (single source)

`CMakeLists.txt` `project(LocalWisperFlow VERSION x.y.z)` is the single
version source:

- `src/core/AppVersion.h` gets it via the `LWF_APP_VERSION` compile
  definition (About page, crash reports, update check).
- `build/EchoFlow.rc` (VERSIONINFO) is configured from it at build time.
- The Inno Setup installer accepts it via `iscc /DAppVersion=x.y.z`.
- `scripts/package_windows.ps1` extracts it from CMakeLists.txt for the zip
  name.

Bump the project version in the release commit, then tag.

## 2. Pre-tag gates

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
ctest --test-dir build --output-on-failure
sh scripts/check_hygiene.sh
sh scripts/check_no_network.sh
sh scripts/check_licenses.sh
```

## 3. Tag and push

```sh
git tag -a vX.Y.Z -m "Echo flow vX.Y.Z"
git push origin vX.Y.Z
```

## 4-9. What release.yml does (windows-latest)

1. Restore the dependency caches (`fetch_deps.ps1` tarballs).
2. Two-phase configure + build (llama_ext first, then everything).
3. `ctest -C Release` on the Windows build.
4. `scripts/package_windows.ps1` -> `dist/EchoFlow-x.y.z-win64.zip` (with the
   import-table audit: the package fails if the exe needs a non-system DLL).
5. `iscc /DAppVersion=x.y.z installer/lwf.iss` ->
   `dist/EchoFlowSetup-x.y.z.exe` (the online installer; models download on
   first run). The full/offline variant compiles the same script with
   `/DFullBundle` after the model weights are placed in the staging dirs.
6. Signing (conditional): if the `SIGNING_CERT_B64` + `SIGNING_CERT_PASSWORD`
   secrets are present, the exe and the installer are Authenticode-signed:

   ```powershell
   signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /f cert.pfx /p <password> <file>
   ```

   Without the secrets the step is skipped and the release ships unsigned
   (SmartScreen shows "Windows protected your PC"; recipients use
   "More info" -> "Run anyway" - documented in the zip README).
7. `SHA256SUMS.txt` over the installer + zip (signed releases also publish
   the signature files).
8. `latest.json` is generated and attached:
   `{"version": "x.y.z", "released_at": "<iso8601>", "installer_url": ...,
   "portable_url": ..., "sha256": {"installer": ..., "portable": ...}}` -
   the file the in-app update check reads.
9. A GitHub Release draft with all artifacts; publish it after eyeballing
   the assets.

## Code signing without paying (current path)

- **SignPath Foundation** provides free code signing for open-source
  projects (public repo, application + review required). When approved, wire
  their GitHub Action in place of the raw `signtool` step and record the
  certificate subject in SECURITY.md.
- Interim: unsigned releases + SHA256SUMS (the workflow does this today).
- Self-signed certificates are free but do NOT remove the SmartScreen
  warning (Windows only trusts them on machines that installed the cert) -
  useful only to test the signing step itself.
- Paid options (Azure Trusted Signing ~$10/mo, OV/EV certificates) are
  documented here for completeness but are not used while the project is
  zero-budget.

## After publishing

- Verify `latest.json` on the release is fetchable and its URLs resolve.
- In-app check: Settings > General > "Check for updates" should report the
  new version on the previous release's build.
- Update NOTICE.md if the dependency set changed; run `check_licenses.sh`.
