# Gate — full branch (gate-final) — 2026-11-07
Owner: ORCH | Branch: feature/studio-pages | HEAD: 73d9ae0 | Baseline: 0e655ec (1d95fb2 merge + docs cherry-pick) / CI 42310536369 both jobs success.
| Step | Command | Result | vs baseline |
|---|---|---|---|
| Build | cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build --config Release --parallel 4 | configure exit 0; build exit 0 | none new |
| Typecheck | zig c++ (Windows files) | NOT RUN (no zig on box) — pending per task, recorded honestly | pending |
| Lint | scripts/check_hygiene.sh | OK | unchanged |
| Network | scripts/check_no_network.sh | OK (D2/D4 allowlist edits only) | intended diff: P02/A1/C2 |
| Unit + integration | ctest --test-dir build --output-on-failure | 100% passed out of 23, 1178.41s | none new (baseline 23/23) |
New failures (regressions): none. Pre-existing: none. Flaky: none observed.
Triage/action: none. G3 zig + Windows runtime + live-key checks remain NOT verified by design.
