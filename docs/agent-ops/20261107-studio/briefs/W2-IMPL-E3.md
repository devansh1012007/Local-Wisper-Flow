# Brief — W2-IMPL-E3 (implementer) · wave 2 · run 20261107-studio

## 1. Role and mission
Mission: Scratchpad injection target (P16) + ApiKeyStore (A1/T-022) + the main.cpp integration pass that wires Wave 2 together. Root cause: dictation cannot target an editor buffer; keys are env-only.
Role: implementer.

## 2. Required reading (in order)
1. docs/agent-ops/20261107-studio/templates/STANDING-ORDERS.md
2. AGENTS.md (no code comments; keys store→env D3 — api_keys.json never config.json, redacted/masked; windows.h only in src/os/; never break config.json additive-only)
3. docs/agent-ops/20261107-studio/00-mission.md, 01-work-ledger.md
4. Approved plan: §§D (scratchpad target writes into editor buffer), E (P16, A1), H, I. Red-team #11: Overlay note button needs an OpenScratchpad route + dirty_ wake.
5. Investigation: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-keys-openrouter.md (ConsentGate env table + api_key_source wiring at main.cpp:398,445-449 — intercept BOTH), INV-ui-shell.md (Overlay dirty_, MainWindow exposes no routing API — you add OpenScratchpad handling in main.cpp, not in MainWindow this wave), INV-enginehooks.md (ITextInjector seam — where the scratchpad target plugs in).
6. Wave 1: reports/w1-impl/W1-IMPL-02E-T-009.md (NotesStore API), W1-IMPL-02B (StatsStore), W1-IMPL-02D (TransformStore — read for the transform NOT-recorded check).

## 3. Task / acceptance (T-013 + T-022)
- New injection target "scratchpad": writes the final transcript into the Scratchpad editor buffer (owned by NotesStore/editor window — Wave 4 builds the window; your target must be testable headlessly: inject into the buffer object, assert text lands there and NOWHERE else — T-never-elsewhere: no focus-steal, no clipboard use unless spec says so; Wave 2 test proves it with a fake editor buffer + real DictationEngine hooks).
- UiCommandType::OpenScratchpad (additive; UiBus has no default case — add the consumer in main.cpp this wave since you own main.cpp) + Overlay mini note button (Overlay.cpp: set dirty_, route activate → OpenScratchpad) shown only when Settings > Scratchpad > "Show in Flow Bar" is ON (config key additive, default OFF — extend BuildSchema minimally; that file is hot — do it in main.cpp-adjacent config accessor or post NEED if BuildSchema is locked by a sibling... BuildSchema is YOUR edit in this wave; announce via COMMS first).
- ApiKeyStore (T-022): src/core/ApiKeyStore.{h,cpp} — api_keys.json next to config.json (never in config.json); entry per provider (GNANI/ELEVENLABS/META + future OPENROUTER reserved key name only); masked in GUI (Wave 4 — your store exposes presence bool, never prints keys); ConsentGate api_key_source reads store→env (store first, env fallback); redaction via lwf::RedactSecret on every stored log line; file-write allowlist NEED for scripts/check_no_network.sh (02A-owned this wave? No — Wave 2 has no single gate owner; YOU add ApiKeyStore.cpp to the file-write allowlist in the SAME commit as the .cpp — you are the gate owner for this wave; check Wave 1's exact `grep -vE` line first, extend it, don't touch the network-API pattern or endpoint regex).
- main.cpp integration (you own it): wire StatsStore (nullable into DictationEngine per E1's ctor contract — check C:\Users\devan\.opencode\plan + E1's NEED lines in COMMS; if E1 landed, wire its store; else wire defensively), NotesStore, ApiKeyStore→ConsentGate, OpenScratchpad consumer, Overlay button. Keep RunEngine dead (do NOT wire it). Preserve ALL existing behavior ordering.
- Tests: scratchpad-never-elsewhere; ApiKeyStore store→env precedence (store wins; empty store falls back to env; missing file = env only; redaction: no key bytes in log lines); Overlay button dirty_ wake; existing engine/config tests unchanged (diff proves it).
Acceptance: scoped lwf_tests green for new filters; hygiene + no_network OK (your gate edit in same commit); main.cpp wires with explicit line refs in report; RunEngine untouched.

## 4. Ownership
WRITE-SET: src/core/ApiKeyStore.{h,cpp}, src/core/UiBus.{h,cpp} (OpenScratchpad additive), src/core/NotesStore.{h,cpp} (only if target needs a helper — else read), src/core/SettingsStore.{h,cpp} (additive keys only: scratchpad_flow_bar), src/core/ConsentGate.{h,cpp} (api_key_source), src/ui/Overlay.{h,cpp} (button), src/main.cpp (integration), scripts/check_no_network.sh (ApiKeyStore allowlist ONLY), tests/test_scratchpad_target.cpp (new), tests/test_apikeystore.cpp (new)
READ-ONLY: everything else.
FORBIDDEN: CMakeLists.txt (post NEED for integrator — or register your new tests if the file is free? CMakeLists is Wave-1-hotspot; Wave 2 has no declared owner — declare: YOU edit CMakeLists.txt for your two new test files ONLY, first, and declare the head in COMMS; if you collide with a sibling doing the same, CONFLICT), src/os/**, src/ui/MainWindow.*.

## 5. Siblings
E1 (stats hooks — DictationEngine.cpp:475 region: FORBIDDEN to you) and E2 (transform flow: FORBIDDEN to you; UiBus.h shared — both additive; declare hunks in COMMS CLAIM before editing). Your main.cpp wiring must apply LAST in spirit — if E1/E2 post NEED for main.cpp lines, apply them in your pass (single writer).

## 6. Traps
Never emit key bytes (logs, exceptions, test output — redact in tests too). Never write api_keys.json to config.json. Never wire RunEngine. Never break config additive-only rule. No code comments.

## 7. Environment
Scoped: cmake --build build --config Release --parallel 4 --target lwf_tests; ctest filtered; bash scripts/check_hygiene.sh; bash scripts/check_no_network.sh. Never full ctest.

## 8. Deliverables
Report: docs/agent-ops/20261107-studio/reports/w2-impl/W2-IMPL-E3-T-01322.md (ORCH-DIGEST first). Commits explicit paths only; trailers Task: T-013,T-022 / Agent: W2-IMPL-E3 / Wave: 2 / Report: <path>. Final <=8 lines.
