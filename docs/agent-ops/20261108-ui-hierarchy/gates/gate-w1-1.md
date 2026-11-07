# Gate — wave 1 run 1 — 2026-11-08
- Tree: git status --porcelain → M COMMS.md only (orchestrator WAVE-START append, pending commit)
- HEAD: b9b836e (Wave 1 landed; WAVE_BASE_SHA 2c06061)
- Baseline: 00-mission.md (ctest 1/1 Passed 23.98s, hygiene OK, no-network OK, full build timed out)

| Step | Command | Result | vs baseline |
| Build lwf_tests | cmake --build build --target lwf_tests --parallel 4 | Built target lwf_tests | match |
| Build full exe | cmake --build build --target local-wisper-flow --parallel 4 (300s) | Built target local-wisper-flow | NEW: baseline timed out at 120s, now passes (incremental, previously slow first build) |
| Unit ctest | ctest --test-dir build -R lwf-tests | 1/1 Passed 19.76s, 100% tests passed | match (green) |
| Hygiene | bash scripts/check_hygiene.sh | hygiene: OK | match |
| No-network | bash scripts/check_no_network.sh | no-network: OK | match |
| G3 zig | not run | pending (no zig on dev box) | pending, per QUESTIONS |
| New/changed tests ×1 | W1-IMPL-03 fail-before proven by counts (see report W1-IMPL-03 section 7); wave ctest passes | green | n/a |

New failures (regressions): none. Pre-existing failures unchanged: full-build timeout at 120s resolved by longer timeout/incremental — note as baseline correction, not regression.
Flaky: none observed.
Triage/action: proceed to independent review (F). Screenshots P-006 still pending manual Windows capture.
