# Brief — W3-IMPL-CI · wave 3 (follow-up) · run 20261106-gui

Mission: fix pre-existing CI dependency-provision failures (NOT caused by wave work). Read STANDING_ORDERS, AGENTS.md, and the CI investigation digest (command log + full report above).

AC (measurable):
- Linux phase-1: lwf_core compiles without _deps-src present (Fix A minimal: FetchContent miniaudio outside FULL, or gate TonePlayer.cpp/miniaudio link to FULL — your judgment, document choice).
- Windows: fix cares failure (fetch_deps-produced -full tarball + file:// GRPC_URL, or submodule init, or FetchContent overrides; must honor pinned SHAs in scripts/fetch_deps.ps1).
- Pushed branch CI: both jobs past Configure + Build phases (ctest green if reached).
- Local gates still green: build local-wisper-flow, ctest -R lwf-tests, hygiene.

WRITE-SET: CMakeLists.txt, .github/workflows/ci.yml, scripts/fetch_deps.sh, scripts/fetch_deps.ps1 ONLY (plus report). FORBIDDEN: src/ code behavior changes, _deps-src, tests/.

Verify: local cmake build + ctest + hygiene; push new commit to fix/gui-typography-dpi and observe gh run. Do not merge.

Deliverable: report docs/agent-ops/20261106-gui/reports/w3-impl/W3-IMPL-CI-T-008.md + code commit then docs commit, trailers Task: T-008, Agent: W3-IMPL-CI, Wave: 3.
