# Brief — W1-IMPL-INT (integrator) · wave 1 · run 20261107-studio

## 1. Role and mission
Mission: wire Wave 1 into the build — register all new sources + tests in CMakeLists.txt, extend scripts/check_no_network.sh with the DEFERRED-DEPENDENCY lines, then run the wave gate (build + ctest + hygiene + no_network). Root cause: 5 parallel coders left CMake/gate wiring to you (hotspot single-writer).
Role: integrator (you own the hotspots this wave).

## 2. Required reading
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md (binding; you are the only CMakeLists/scripts writer this wave)
2. AGENTS.md (gates: build -j4, ctest, check_hygiene.sh, check_no_network.sh; no code comments)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§E (P02-P11 rows), §H, §I.
5. Reports: all Wave 1 implementer reports in docs/agent-ops/20261107-studio/reports/w1-impl/ (read digests + NEED/COMMS lines for gate additions).
6. COMMS.md (tail): NEEDs for check_no_network.sh allowlist — expected: StatsStore.cpp, DictionaryStore.cpp, SnippetStore.cpp, TransformStore.cpp, NotesStore.cpp (+ .h where the script pattern needs it). AtomicFile already landed by 02A — verify, don't duplicate.

## 3. Task / acceptance
- CMakeLists.txt: add new sources to lwf_core (AtomicFile.cpp, TextDiff.cpp, InsightsModel.cpp, StatsStore.cpp, DictionaryStore.cpp, TransformStore.cpp, NotesStore.cpp; modified SnippetStore/PersonalDictionary/AppContextResolver already listed) + new test files to lwf_tests target. Group your edit cleanly.
- scripts/check_no_network.sh: add exactly the DEFERRED files produced by NEW .cpps that write files (extend the existing `grep -vE` allowlist line — do not touch the network-API pattern or endpoint regex beyond what's needed).
- Run the gate (no other writers active): cmake -S . -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j4 (or --parallel 4); ctest --test-dir build --output-on-failure; bash scripts/check_hygiene.sh; bash scripts/check_no_network.sh. Record verbatim results in your report AND write docs/agent-ops/20261107-studio/gates/gate-w1-1.md (gate template in skill appendix J format: tree state, HEAD, step table, regressions vs baseline CI 42310536369).
- If the build/test fails on files you don't own, write the error + owning agent in your report — do NOT edit those files; post NEED. If failures are pre-existing (present at BASE 0e655ec), say so with evidence (git stash-free check: compare against origin/main behavior).
- Windows-only files: note GO will be type-checked (no zig here) — record honestly, never "works".
Acceptance: build + ctest + both scripts green (or failures proven pre-existing with evidence); gate file written; report includes per-test results for new suites (counts: AtomicFile, TextDiff, InsightsModel, StatsStore, DictionaryStore, SnippetStore v2, TransformStore, NotesStore).

## 4. Ownership
WRITE-SET: CMakeLists.txt, scripts/check_no_network.sh, docs/agent-ops/20261107-studio/gates/gate-w1-1.md
READ-ONLY: everything else. FORBIDDEN: src/**, tests/**.
Sibling 02E runs concurrently on src/core/NotesStore.* only — do not wait for it; wire what's present (NotesStore may land during your run; if its files appear after your CMake edit, note it and either include them if present at your final edit or report a FOLLOW-UP need — do NOT extend your window by polling).

## 5. Environment
You ARE the gate (full suite allowed since integrators set env for gate). Kill processes you start. No containers here (native build).

## 6. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w1-impl/W1-IMPL-INT.md (ORCH-DIGEST first). Commits: CMakeLists+scripts first, then report+gate docs (trailers Task: T-002..T-010-integration / Agent: W1-IMPL-INT / Wave: 1 / Report: <path>). Final <=8 lines.
