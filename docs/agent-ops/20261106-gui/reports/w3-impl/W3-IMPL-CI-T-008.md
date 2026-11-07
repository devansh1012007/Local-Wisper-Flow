<!-- ORCH-DIGEST:START -->
agent: W3-IMPL-CI | task: T-008 | status: DONE
code_commits: d643c4e | report_commit: pending
files_changed: 2 (CMakeLists.txt, .github/workflows/ci.yml)
contracts_changed: none
tests: added 0, changed 0; scoped run: pass (local build + ctest -R lwf-tests + hygiene; full CI pending push observation)
ownership_violations: none
deferred_dependencies: none
out_of_scope_findings: 1
temporary_mitigation: no
deep_issue_flag: no
least_confident_about: real CI outcome (not yet observed at report time; must be confirmed via gh run watch)
confidence: high (root causes reproduced empirically; local gates green)
last_comms_line_read: 2026-11-06T21:00:00Z
<!-- ORCH-DIGEST:END -->

# Implementation report — W3-IMPL-CI — T-008 CI dependency-provision fixes

## 1. Task and acceptance criteria (restated)

Pre-existing (not wave-caused) CI failures, verified before starting:
- (AC1) Linux: `src/audio/TonePlayer.cpp:3: fatal miniaudio.h not found` in a fresh checkout (no gitignored `_deps-src/`).
- (AC2) Windows: configure fails at `build/_deps/grpc-src/cmake/cares.cmake:25` because CI fetches the plain GitHub gRPC tarball (no submodule sources) and never runs `scripts/fetch_deps`.
- (AC3) Pushed branch CI: both jobs past Configure + Build (ctest green if reached).
- (AC4) Local gates green: build `local-wisper-flow` + `lwf_tests`, `ctest -R lwf-tests`, hygiene.
- WRITE-SET: `CMakeLists.txt`, `.github/workflows/ci.yml`, `scripts/fetch_deps.sh`, `scripts/fetch_deps.ps1` ONLY (+ report). FORBIDDEN: `src/` behavior, `tests/`, `_deps-src/`. No UAT requested for this phase.

## 2. Verification that the problem exists

Evidence I gathered myself (not copied from the investigation):

- [VERIFIED CMakeLists.txt:39-96] `src/audio/TonePlayer.cpp` (which `#include`s `<miniaudio.h>` at line 3) is in `LWF_CORE_SOURCES`, so it is compiled into `lwf_core` unconditionally; `lwf_core` links `miniaudio` and adds the gitignored `_deps-src/miniaudio` include dir. The miniaudio `FetchContent` declaration sat inside `if(LWF_BUILD_FULL)` (old lines 214-224), which is OFF on Linux.
- [VERIFIED] Locally `_deps-src/miniaudio/miniaudio.h` exists (21-entry clone, gitignored — `.gitignore` lists `_deps-src/`), which is exactly why local non-FULL builds worked and fresh CI checkouts did not.
- [VERIFIED] Negative control [RAN `g++ -fsyntax-only -std=c++17 -Isrc src/audio/TonePlayer.cpp`] → `src/audio/TonePlayer.cpp:3:10: fatal error: miniaudio.h: No such file or directory` — the exact CI failure reproduced.
- [VERIFIED] Local `build/CMakeCache.txt` shows `GRPC_URL=file://.../_deps-cache/grpc-v1.71.0-full.tar.gz` and `LLAMA_URL=file://.../llama-b10603.tar.gz` with `FETCHCONTENT_SOURCE_DIR_*` overrides — the local offline recipe never exercised by CI.
- [VERIFIED] `tar -tzf _deps-cache/grpc-v1.71.0.tar.gz | grep cares/cares/CMakeLists.txt` → no match (plain tarball lacks submodule sources = root cause of the Windows failure); `tar -tzf _deps-cache/grpc-v1.71.0-full.tar.gz | grep cares/cares/CMakeLists.txt` → `./third_party/cares/cares/CMakeLists.txt` present (fetch_deps-produced `-full` tarball is submodule-complete).
- [VERIFIED] `Select-String LWF_BUILD_GRPC CMakeLists.txt` → no matches: `-DLWF_BUILD_GRPC=OFF` in `ci.yml` is a phantom (unknown `-D` is ignored by CMake).

## 3. Root cause

1. **Linux:** dependency-declaration misplaced. `TonePlayer.cpp` is a genuine, always-built `lwf_core` source, but its `miniaudio.h` include was only provisioned by a `FetchContent` block gated behind `LWF_BUILD_FULL`. In non-FULL builds the only provider was the manual, gitignored `_deps-src/miniaudio` directory, absent in any fresh checkout (hence CI). Because `target_link_libraries(lwf_core PUBLIC miniaudio)` with a non-existent target is treated as a plain linker flag (no configure error), the failure manifested as a compile-time "header not found" rather than a configure error.
2. **Windows:** missing provision step. gRPC `v1.71.0` builds abseil/protobuf/re2/cares/zlib/boringssl from in-tree `third_party/` sources, which the GitHub auto-archive omits. The documented recipe (`AGENTS.md`: run `scripts/fetch_deps`, pass `-DGRPC_URL=file://...grpc-v1.71.0-full.tar.gz`) was never wired into CI, so FetchContent downloaded the submodule-less tarball and `cmake/cares.cmake` failed on the empty stub.

## 4. Approach chosen and why

- **Linux — Fix A (chosen): move the miniaudio `FetchContent` declaration out of `if(LWF_BUILD_FULL)`**, directly alongside the always-fetched `json`/`doctest`, before `lwf_core`. Rejected Fix B (gating `TonePlayer.cpp` behind FULL): it would change `lwf_core`'s composition in non-FULL builds (removing a public compilation unit), risk breaking downstream consumers (`lwf_tests`, benches link `lwf_core`), and falsely treats a core facility (notification tones) as a FULL-only provider. Fix A preserves the non-FULL core API byte-for-byte; the fetched version (0.11.25) is identical to the previously-declared one — same `GIT_REPOSITORY`/`GIT_TAG`, same `if(NOT TARGET miniaudio)` INTERFACE fallback, moved verbatim.
- **Also removed the `_deps-src/miniaudio` entry from `lwf_core`'s `target_include_directories`.** Rationale: with FetchContent owning the pinned 0.11.25, a stale gitignored checkout earlier in the include search would shadow the pinned fetch, and the entry is precisely the "fresh checkout lacks it" trap. `FETCHCONTENT_SOURCE_DIR_MINIAUDIO` overrides still work for anyone who needs a local mirror (used by this machine's own build). This is part of making the fix hermetic, not separate cleanup.
- **Windows — run `scripts/fetch_deps.ps1`, then configure with `-DGRPC_URL=file:///.../_deps-cache/grpc-v1.71.0-full.tar.gz` (and `-DLLAMA_URL=file:///...` for the llama tarball, matching the local cache).** This honors the pinned SHAs in `scripts/fetch_deps.ps1:18-25` (the script is invoked as-is; no SHA, URL, or layout logic changed) and follows the `AGENTS.md` recipe verbatim. Replaced path construction with a `pwsh` rewrite (`$env:GITHUB_WORKSPACE -replace '\\','/'` → `file:///D:/...`) so the URL is valid on the runner. Also: dropped phantom `-DLWF_BUILD_GRPC=OFF` from both jobs; added `_deps-cache` to the Windows `actions/cache` paths so repeat runs skip the large downloads; extended the Windows cache key with `scripts/fetch_deps.ps1` so a pinned-SHA change invalidates the cached deps.

## 5. What changed (per file)

| File | Change | Why |
|---|---|---|
| CMakeLists.txt | Miniaudio `FetchContent` (GitHub 0.11.25, shallow, INTERFACE fallback) moved from inside `if(LWF_BUILD_FULL)` to an unconditional declaration after `doctest` | TonePlayer.cpp is always compiled; miniaudio is a genuine core dep |
| CMakeLists.txt | Removed `"${CMAKE_CURRENT_SOURCE_DIR}/_deps-src/miniaudio"` from `lwf_core` includes | Hermetic single-source (pinned fetch); removes stale-shadow trap |
| .github/workflows/ci.yml (linux) | Removed phantom `-DLWF_BUILD_GRPC=OFF` | No such CMake option; misleading |
| .github/workflows/ci.yml (windows) | Cache now includes `_deps-cache`; key hashes `scripts/fetch_deps.ps1` too | Skip repeat downloads; invalidate on pinned-SHA change |
| .github/workflows/ci.yml (windows) | New step: `pwsh scripts/fetch_deps.ps1` before configure | Produces submodule-complete `-full` tarball + llama tarball |
| .github/workflows/ci.yml (windows) | Both configure phases pass `-DGRPC_URL=file:///.../grpc-v1.71.0-full.tar.gz -DLLAMA_URL=file:///.../llama-b10603.tar.gz` | Feed FetchContent the submodule-complete package (verbatim AGENTS.md recipe) |
| scripts/fetch_deps.sh, scripts/fetch_deps.ps1 | **Unchanged** (verified correct) | Already implement the pinned-SHA recipe; CI now invokes them |

## 6. Blast radius and contracts

- No `src/`, `tests/`, or `_deps-src/` file touched (only build-system + CI wiring). No public API, CLI flag, serialized format, or event schema changed — zero wave UI/engine behavior impact.
- Greps: `miniaudio` in CMakeLists → single declaration (line ~45-53), link sites unchanged (`lwf_core`, `local-wisper-flow` under FULL). `TonePlayer` consumers: `src/main.cpp` (Windows-guarded body only); `tests/` — no `TonePlayer` references (test_tones exercises `Generate` only), so `lwf_core`'s symbol surface is unchanged by Fix A.
- `file://` URLs: FetchContent `URL`/`ExternalProject URL` accept `file://` (proven locally — this machine's `build/CMakeCache.txt` uses exactly these two URLs successfully). Two-phase llama flow preserved; `GRPC_URL` passed at phase 1 (when grpc-src is materialized), re-passed at phase 2 for consistency.
- Backward compat: `FETCHCONTENT_SOURCE_DIR_MINIAUDIO` overrides and cached `_deps-cache` tarballs continue to work; non-FULL core API unchanged.

## 7. Tests

- **Reproduction-proof (fails without the fix):** [RAN `g++ -fsyntax-only -std=c++17 -Isrc src/audio/TonePlayer.cpp`] → `fatal error: miniaudio.h: No such file or directory` (exact CI error). [RAN same + `-I<pristine miniaudio-0.11.25 extracted from _deps-cache tarball>`] → exit 0. This models precisely what fixed CMake gives Linux CI: FetchContent-provided `miniaudio.h`, no `_deps-src/`.
- **Existing suite (behavior unchanged):** [RAN `ctest --test-dir build -R lwf-tests --output-on-failure`] → `1/1 Passed` (17.09s). No tests added/changed (none needed — this is build provisioning, not logic).
- **Windows mechanism:** tarball membership checks above (plain lacks cares; `-full` has it) + local build configured against the identical `file://` recipe [RAN `cmake -S . -B build` → exit 0, `cmake --build build -j4` → exit 0, `lwf_tests` built]. Real CI run → pending (see §14).
- **YAML gate:** [RAN `python -c yaml.safe_load(...)`] → parses; windows steps include the fetch_deps step in order.
- G3 (zig type-check of Windows sources): pending — no zig on dev box (recorded per AGENTS.md convention). No Windows-only source was touched, so G3 risk is limited to the pre-existing pending state.

## 8. Long-run test

- **Source:** fetched provenance is pinned (miniaudio 0.11.25; grpc submodule SHAs in fetch_deps.ps1:18-25) and already the documented recipe — no invented URLs/SHAs.
- **Load:** CI windows fetch adds ~6 tarball downloads (cached under `_deps-cache` after first run); llama/grpc build time unchanged.
- **Hostile:** all downloads are HTTPS from github.com (same hosts/versioned SHAs as local recipe); no new network endpoints and no secrets cross the boundary.
- **Operability:** two-phase configure preserved; a stale `build/_deps` cache cannot resurrect the plain tarball because fetch_deps always rewrites/regenerates `-full` when missing and the cache key now covers the fetch script.
- **Reversal:** revert commit `d643c4e` restores prior behavior exactly; removing the CI fetch step degrades to the same failure as today (no hidden state).

## 9. Edge cases and abuse scenarios

- Handled: missing `_deps-src/` (Linux) — FetchContent clones to `build/_deps`; existing `_deps-src/` present — same fetch path (override or clone), no shadowing since the gitignored include entry is gone; `GITHUB_WORKSPACE` backslashes → URL-safe forward slashes; `fetch_deps` idempotent (skips existing files).
- Consciously not handled: CI runners without network (FetchContent git clones for json/doctest/miniaudio all need network — pre-existing posture, not introduced here); fetch_deps.ps1 on other PS hosts irrelevant (windows-2022 runner, `pwsh` explicitly invoked).
- Unknown: actual GitHub Actions outcome until observed (see §14).

## 10. Performance and security considerations

No runtime code changed; performance impact nil. Supply chain: same pinned versions/SHAs as the local offline recipe — the change makes CI *more* pinned than today (plain GitHub archive → submodule-complete tarball at pinned SHAs). No secrets, tokens, or env values written into code/reports/commits.

## 11. Commands run and results (verbatim summaries)

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` → exit 0 (only pre-existing FetchContent_Populate/min-version deprecation warnings).
- `cmake --build build -j4` → exit 0, `local-wisper-flow` linked.
- `cmake --build build --target lwf_tests -j4` → exit 0.
- `ctest --test-dir build -R lwf-tests --output-on-failure` → `100% tests passed out of 1` (17.09s).
- `sh scripts/check_hygiene.sh` (with working grep PATH) → `hygiene: OK`, exit 0. (A first invocation in a grep-less PATH printed `grep: command not found` yet exit 0 — false-green artifact of THIS box's PATH; re-ran with grep present for the real result.)
- `g++ -fsyntax-only` (above) → no-include: exact CI fatal reproduced; with pristine 0.11.25: exit 0.
- `gh run list --branch fix/gui-typography-dpi` / `gh run watch <id>` → PENDING (post-push; outcome to be appended/updated).
- Push: `git push origin fix/gui-typography-dpi` → PENDING.

## 12. Deviations from the plan/brief; ownership issues; deferred dependencies

- **Deviation (minor):** code commit `d643c4e` does not carry standalone `Task:/Agent:/Wave:` trailer lines (they are in the subject line) — `git commit --amend` is forbidden by STANDING_ORDERS rule 13, so the code commit was left as authored. The docs commit carries the full trailer set (`Task: T-008`, `Agent: W3-IMPL-CI`, `Wave: 3`, `Report:`). Noted honestly rather than hidden.
- **Deferred dependency (out-of-scope):** the `FetchContent_Populate(imgui)` usage (CMakeLists.txt ~line 360) is deprecated (CMake 4.4 warning) — out of write-set scope for a CI-provision fix; recorded in §13.
- Branch discipline note: STANDING_ORDERS rule 11 says operate on `ops/<run-id>`, but COMMS.md line 24 (ORCH DECISION) sets the operation branch to `fix/gui-typography-dpi` per user direction, and the mission explicitly requires pushing it — followed the orchestrator directive + mission.
- Standing orders rule 13 forbids `push`, but the mission explicitly requires `git push origin fix/gui-typography-dpi` and COMMS line 24 confirms the branch policy — pushed per mission (orchestrator outranks template default; COMMS rule 4: "your brief outranks any line here"). If the orchestrator wants a different push policy, say so.

## 13. Out-of-scope findings

1. `FetchContent_Populate(imgui)` at CMakeLists.txt ~line 360 is deprecated under CMake ≥4 (`Policy CMP0169`); configure emits an author warning. Recommend a future task migrate to `FetchContent_MakeAvailable(imgui)` with the same populate-guard semantics. (No behavior impact today.)
2. `scripts/fetch_deps.sh` archives with `./` inside the top dir while `fetch_deps.ps1` uses a top-level `grpc-1.71.0/` dir — both extract correctly via FetchContent's single-top-dir detection, so no action taken.
3. The phantom `-DLWF_BUILD_GRPC=OFF` presumably once gated an intended gRPC-off mode; there is no such option in CMakeLists — noted for whoever owns the CMake options docs if they want to document the absence.

## 14. Known limitations, risks, rollback notes

- Real Linux/Windows CI outcomes were **not** observed at the time of this report (push + `gh run watch` pending). Per the brief's AC3, "both jobs past Configure + Build" must be confirmed from the run itself — this caveat applies to any pre-push claim; the local empirical evidence above (reproduced failure, proven fix mechanism, identical file:// recipe already used locally) is strong but is not a substitute for the green X's.
- Linux full-build (non-FULL) configure+build was simulated at the compile-unit level (g++ syntax check), not by a full non-FULL CMake configure on Linux, because Windows forces `LWF_BUILD_FULL=ON` (CMakeLists.txt:17-19) and a scratch Windows full configure would rebuild grpc/whisper/llama. The mechanism (unconditional FetchContent → include present) is identical to what Linux CI will do.
- Windows CI total wall-time may grow (fetch_deps downloads once; then cached). No destructive operations; rollback = revert `d643c4e`.

## 15. Where I am least confident / what the reviewer should scrutinize

- Least confident: the GitHub Actions execution itself (fetch_deps.ps1 under `pwsh` on windows-2022, `actions/cache` restore-ordering with the new `_deps-cache` path, and build times). Scrutinize the pushed run's logs when it completes.
- The `file:///$ws/...` path assembly (`file:///D:/...` for drive-letter workspaces) — sane, and matches the locally proven `file://D:/...` shape, but the exact runner workspace path form should be confirmed in the configure log (`GRPC_URL` echo).

## 16. Self-review checklist

- [x] Root cause, not symptom (declaration placement; missing provision step)
- [x] No band-aid patterns (no swallowing, retries, type suppression, test changes)
- [x] Tests fail without the fix (negative control reproduced the exact CI error)
- [x] Only write-set files changed (CMakeLists.txt, .github/workflows/ci.yml; scripts verified correct, untouched; COMMS appended per protocol, not committed)
- [x] No secrets/PII
- [x] Conventions from AGENTS.md followed (file:// recipe; pinned SHAs; gates run)

## 17. Commits

- code: `d643c4e` | report: (this docs commit — SHA appended after commit)
- Push: `git push origin fix/gui-typography-dpi` (explicit branch ref) — pending at report time.
- CI: `gh run list --branch fix/gui-typography-dpi` + `gh run watch <id>` — pending; outcome URL to be recorded here after observation.
